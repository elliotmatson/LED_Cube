#include "analytics.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <time.h>

#include "config.h"
#include "sysinfo.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace
{
    // How often the open dashboard's statistics refresh, and memory is logged.
    constexpr uint32_t UPDATE_MS = 5000;
    constexpr uint32_t LOG_MS = 10000;
    // The public address rarely changes; a failed lookup (no internet yet,
    // service down) is retried sooner.
    constexpr uint32_t PUBLIC_IP_REFRESH_MS = 30 * 60 * 1000;
    constexpr uint32_t PUBLIC_IP_RETRY_MS = 2 * 60 * 1000;

    String bytes(uint32_t n)
    {
        char text[16];
        sysinfo::formatBytes(n, text, sizeof(text));
        return text;
    }
}

Analytics::Analytics(ESPDash &dashboard)
    : dashboard(dashboard),
      network(dashboard, "WiFi Network"),
      accessPoint(dashboard, "Access Point"),
      localIp(dashboard, "Local IP"),
      publicIpStat(dashboard, "Public IP"),
      macAddress(dashboard, "MAC Address"),
      uptime(dashboard, "Uptime"),
      lastReset(dashboard, "Last Reset"),
      localTime(dashboard, "Local Time"),
      pattern(dashboard, "Current Pattern"),
      frameRate(dashboard, "Frame Rate (fps)"),
      freeInternal(dashboard, "Free Internal RAM"),
      minFreeInternal(dashboard, "Lowest Free Internal RAM"),
      largestInternal(dashboard, "Largest Internal Block"),
      freePsram(dashboard, "Free PSRAM"),
      chipTemperature(dashboard, "Chip Temperature (°C)")
{
}

void Analytics::begin(Renderer &renderer, std::function<String()> patternName)
{
    this->renderer = &renderer;
    this->patternName = patternName;

    macAddress.setValue(WiFi.macAddress());
    lastReset.setValue(sysinfo::resetReasonName(int(esp_reset_reason())));

    network.setProvider([]()
                        { return WiFi.isConnected() ? WiFi.SSID() : String("not connected"); });
    accessPoint.setProvider([]()
                            { return WiFi.isConnected() ? String(WiFi.BSSIDstr() + " (ch " + int(WiFi.channel()) + ", " + int(WiFi.RSSI()) + " dBm)") : String("-"); });
    localIp.setProvider([]()
                        { return WiFi.isConnected() ? String(WiFi.localIP().toString() + " (" HOSTNAME ".local)") : String("-"); });
    publicIpStat.setProvider([this]()
                             {
        String ip = publicIp();
        return ip.length() ? ip : String("unknown"); });
    uptime.setProvider([]()
                       {
        char text[24];
        sysinfo::formatUptime(uint32_t(esp_timer_get_time() / 1000000), text, sizeof(text));
        return String(text); });
    localTime.setProvider([]()
                          {
        struct tm now;
        char text[32] = "not set yet";
        if (getLocalTime(&now, 0))
        {
            strftime(text, sizeof(text), "%Y-%m-%d %H:%M %Z", &now);
        }
        return String(text); });
    pattern.setProvider([this]()
                        { return this->patternName(); });
    frameRate.setProvider([this]()
                          {
        Renderer::Stats s = this->renderer->stats();
        return s.windowMs ? s.frames * 1000.0f / s.windowMs : 0.0f; });
    freeInternal.setProvider([]()
                             { return bytes(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); });
    minFreeInternal.setProvider([]()
                                { return bytes(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)); });
    largestInternal.setProvider([]()
                                { return bytes(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)); });
    freePsram.setProvider([]()
                          { return String(bytes(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + " of " + bytes(heap_caps_get_total_size(MALLOC_CAP_SPIRAM))); });
    chipTemperature.setProvider([]()
                                { return temperatureRead(); });

    // The public IP lookup runs a TLS handshake on this stack; the same size
    // as the Weather worker, which does the same.
    xTaskCreate([](void *o)
                { static_cast<Analytics *>(o)->loop(); },
                "Analytics", 10240, this, 1, &task);
}

String Analytics::publicIp()
{
    char copy[sizeof(publicIpText)];
    portENTER_CRITICAL(&publicIpMux);
    memcpy(copy, publicIpText, sizeof(copy));
    portEXIT_CRITICAL(&publicIpMux);
    return String(copy);
}

void Analytics::loop()
{
    uint32_t lastLog = 0;
    uint32_t nextLookup = 0;
    for (;;)
    {
        const uint32_t now = millis();
        if (WiFi.isConnected() && int32_t(now - nextLookup) >= 0)
        {
            nextLookup = millis() + (lookUpPublicIp() ? PUBLIC_IP_REFRESH_MS : PUBLIC_IP_RETRY_MS);
        }
        if (now - lastLog >= LOG_MS)
        {
            lastLog = now;
            ESP_LOGI("Analytics", "Free Heap: %u / %u, Used PSRAM: %u / %u", unsigned(ESP.getFreeHeap()), unsigned(ESP.getHeapSize()),
                     unsigned(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                     unsigned(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));
            ESP_LOGI("Analytics", "Largest free block in Heap: %u, PSRAM: %u", unsigned(ESP.getMaxAllocHeap()),
                     unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
        }
        // Reads every provider and sends what changed; does nothing when no
        // dashboard is open.
        dashboard.sendUpdates();
        vTaskDelay(pdMS_TO_TICKS(UPDATE_MS));
    }
}

/// Asks a plain-text "what is my IP" service, with a second in case the
/// first is down or rate-limiting.
bool Analytics::lookUpPublicIp()
{
    const char *const URLS[] = {"https://api.ipify.org", "https://icanhazip.com"};
    for (const char *url : URLS)
    {
        NetworkClientSecure client;
        client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
        HTTPClient http;
        http.setTimeout(10000);
        http.setUserAgent("LED-Cube");
        if (!http.begin(client, url))
        {
            continue;
        }
        const int code = http.GET();
        char ip[sizeof(publicIpText)];
        const bool ok = code == HTTP_CODE_OK && sysinfo::parseIp(http.getString().c_str(), ip, sizeof(ip));
        http.end();
        if (ok)
        {
            portENTER_CRITICAL(&publicIpMux);
            memcpy(publicIpText, ip, sizeof(ip));
            portEXIT_CRITICAL(&publicIpMux);
            return true;
        }
        ESP_LOGW("Analytics", "%s: HTTP %d, no IP address", url, code);
    }
    return false;
}
