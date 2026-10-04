#include "lost_mode.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <algorithm>
#include <vector>

namespace
{
    const char *TAG = "Lost";
    const uint32_t FREE_GUESSES = 5;
    const uint32_t LOCKOUT_MS = 15 * 60 * 1000;
    const uint32_t OFFLINE_BEFORE_SEARCH_MS = 3 * 60 * 1000;
    const int MAX_OPEN_TRIES = 5;
}

void LostMode::begin(Settings &s, std::function<bool()> connected)
{
    settings = &s;
    brokerConnected = connected;
    prefs.begin("lost");
    failures = prefs.getUInt("failures", 0);
    // After a restart, a locked-out cube waits a full lockout again.
    lastFailureMs = millis();
    xTaskCreate([](void *self)
                { static_cast<LostMode *>(self)->task(); },
                "Lost", 4096, this, 1, nullptr);
}

std::string LostMode::hash(const std::string &salt, const std::string &pin)
{
    uint8_t digest[32];
    const std::string input = salt + ":" + pin;
    mbedtls_sha256(reinterpret_cast<const unsigned char *>(input.data()), input.size(), digest, 0);
    char hex[65];
    for (int i = 0; i < 32; i++)
    {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return hex;
}

void LostMode::setPin(const std::string &pin)
{
    if (pin.empty())
    {
        settings->setLostPin("");
    }
    else
    {
        char salt[17];
        snprintf(salt, sizeof(salt), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
        settings->setLostPin(std::string(salt) + ":" + hash(salt, pin));
    }
    failures = 0;
    prefs.putUInt("failures", 0);
    settings->flush();
}

bool LostMode::hasPin() const
{
    return !settings->lostPin().empty();
}

LostMode::Unlock LostMode::tryUnlock(const std::string &pin, uint32_t &waitSeconds)
{
    waitSeconds = 0;
    if (!settings->lostMode())
    {
        return Unlock::NOT_LOST;
    }
    const std::string stored = settings->lostPin();
    const size_t colon = stored.find(':');
    if (colon == std::string::npos)
    {
        return Unlock::NO_PIN;
    }
    if (failures >= FREE_GUESSES && millis() - lastFailureMs < LOCKOUT_MS)
    {
        waitSeconds = (LOCKOUT_MS - (millis() - lastFailureMs)) / 1000 + 1;
        return Unlock::LOCKED_OUT;
    }
    const std::string expected = stored.substr(colon + 1);
    const std::string got = hash(stored.substr(0, colon), pin);
    // Constant time: the comparison should not say how much matched.
    uint8_t diff = expected.size() != got.size();
    for (size_t i = 0; i < expected.size() && i < got.size(); i++)
    {
        diff |= uint8_t(expected[i] ^ got[i]);
    }
    if (diff == 0)
    {
        failures = 0;
        prefs.putUInt("failures", 0);
        return Unlock::OK;
    }
    failures++;
    lastFailureMs = millis();
    prefs.putUInt("failures", failures);
    ESP_LOGW(TAG, "Wrong unlock PIN (%lu)", (unsigned long)failures);
    if (failures >= FREE_GUESSES)
    {
        waitSeconds = LOCKOUT_MS / 1000;
    }
    return Unlock::WRONG;
}

void LostMode::task()
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(30000));
        const bool online = WiFi.status() == WL_CONNECTED;
        if (!settings->lostMode() || online)
        {
            offlineSinceMs = 0;
            if (!online)
            {
                _via = "saved";
            }
            continue;
        }
        // The setup portal is up: someone may be about to connect the cube
        // to their own network, the best outcome. Leave it alone.
        if (WiFi.getMode() & WIFI_MODE_AP)
        {
            offlineSinceMs = 0;
            continue;
        }
        if (offlineSinceMs == 0)
        {
            offlineSinceMs = millis();
            continue;
        }
        if (millis() - offlineSinceMs < OFFLINE_BEFORE_SEARCH_MS)
        {
            continue;
        }
        if (!tryOpenNetworks())
        {
            offlineSinceMs = millis(); // wait a while before the next search
        }
    }
}

/// Joins the strongest open networks in turn, keeping the first through
/// which the broker answers. Leaves the saved credentials as they were.
bool LostMode::tryOpenNetworks()
{
    wifi_config_t saved = {};
    esp_wifi_get_config(WIFI_IF_STA, &saved);
    const String savedSsid(reinterpret_cast<const char *>(saved.sta.ssid));
    const String savedPassword(reinterpret_cast<const char *>(saved.sta.password));

    const int16_t found = WiFi.scanNetworks(false, false, false, 150);
    struct Open
    {
        String ssid;
        int32_t rssi;
    };
    std::vector<Open> open;
    for (int i = 0; i < found; i++)
    {
        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN && WiFi.SSID(i).length() > 0)
        {
            open.push_back({WiFi.SSID(i), WiFi.RSSI(i)});
        }
    }
    WiFi.scanDelete();
    std::sort(open.begin(), open.end(), [](const Open &a, const Open &b)
              { return a.rssi > b.rssi; });
    ESP_LOGI(TAG, "Offline while lost: %u open networks in range", (unsigned)open.size());

    for (int i = 0; i < int(open.size()) && i < MAX_OPEN_TRIES; i++)
    {
        ESP_LOGI(TAG, "Trying open network %s (%ld dBm)", open[i].ssid.c_str(), (long)open[i].rssi);
        WiFi.disconnect(false, false);
        // Not persistent: the saved network must survive this.
        WiFi.persistent(false);
        WiFi.begin(open[i].ssid.c_str());
        WiFi.persistent(true);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
        {
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        if (WiFi.status() != WL_CONNECTED)
        {
            continue;
        }
        // Connected is not online: captive portals block the broker.
        start = millis();
        while (!brokerConnected() && millis() - start < 45000)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (brokerConnected())
        {
            _via = String("open:") + open[i].ssid;
            ESP_LOGW(TAG, "Reporting through open network %s", open[i].ssid.c_str());
            return true;
        }
    }

    // Back to the saved network, for when it comes back.
    WiFi.disconnect(false, false);
    WiFi.persistent(false);
    if (savedSsid.length())
    {
        WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    }
    WiFi.persistent(true);
    _via = "saved";
    return false;
}
