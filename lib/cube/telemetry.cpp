#include "telemetry.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_chip_info.h>
#include <esp_core_dump.h>
#include <esp_crt_bundle.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_rom_crc.h>
#include <mbedtls/base64.h>

#include "all_patterns.h"
#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ESP-IDF's certificate bundle (see updates.h).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace
{
    const char *TAG = "Telemetry";

    // Task notification bits.
    const uint32_t WAKE_CONNECTED = 1 << 0;
    const uint32_t WAKE_SETTINGS = 1 << 1;
    const uint32_t WAKE_RESEND_CRASH = 1 << 2;
    const uint32_t WAKE_RESTART = 1 << 3;
    const uint32_t WAKE_LOST = 1 << 4;
    const uint32_t LOCATION_INTERVAL_MS = 2 * 60 * 1000;

    // Scanning takes the radio off its channel for a couple of seconds, so
    // not with every report.
    const uint32_t SCAN_INTERVAL_MS = 60 * 60 * 1000;
    const int MAX_SCAN_RESULTS = 30;

    // Raw core dump bytes per message: base64 makes them 4/3 as many.
    const size_t CRASH_CHUNK = 3072;
    const int MAX_CRASH_CHUNKS = 32;
    portMUX_TYPE crashMux = portMUX_INITIALIZER_UNLOCKED;
    int crashIds[MAX_CRASH_CHUNKS + 1];
    int crashIdCount = 0;

    String hex32(uint32_t v)
    {
        char s[11];
        snprintf(s, sizeof(s), "0x%08lx", (unsigned long)v);
        return s;
    }
}

void Telemetry::begin(Settings &s, BootLog &log, Renderer &r, LostMode &lost, CommandHandler h)
{
    settings = &s;
    lostMode = &lost;
    bootLog = &log;
    renderer = &r;
    handler = h;
    usageLock = xSemaphoreCreateMutex();
    usageStartMs = millis();

    // A random ID, made once: not the MAC, so another cube's topics cannot
    // be guessed from the shared login.
    prefs.begin("telemetry");
    String id = prefs.getString("id", "");
    if (id.length() != 16)
    {
        char made[17];
        snprintf(made, sizeof(made), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
        id = made;
        prefs.putString("id", id);
    }
    strlcpy(deviceId, id.c_str(), sizeof(deviceId));
    base = String("cube/") + deviceId;

#ifdef MQTT_URL
    static String willTopic;
    willTopic = base + "/status";
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = MQTT_URL;
    // Used only for wss:// (and mqtts://); ws:// is for local testing.
    cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
#ifdef MQTT_USER
    cfg.credentials.username = MQTT_USER;
#endif
#ifdef MQTT_PASSWORD
    cfg.credentials.authentication.password = MQTT_PASSWORD;
#endif
    cfg.credentials.client_id = deviceId;
    cfg.session.last_will.topic = willTopic.c_str();
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;
    cfg.session.keepalive = 60;
    cfg.network.reconnect_timeout_ms = 15000;
    cfg.network.timeout_ms = 15000;
    cfg.buffer.size = 2048;
    cfg.buffer.out_size = 6144; // a base64 crash chunk and its JSON
    // Below the patterns' workers and well below the render task.
    cfg.task.priority = 2;
    cfg.task.stack_size = 6144;
    client = esp_mqtt_client_init(&cfg);
    if (client == nullptr)
    {
        ESP_LOGE(TAG, "Could not create the MQTT client");
        return;
    }
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, this);
    xTaskCreate([](void *self)
                { static_cast<Telemetry *>(self)->task(); },
                "Telemetry", 8192, this, 1, &taskHandle);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "Reporting as %s to %s", deviceId, MQTT_URL);
#else
    ESP_LOGI(TAG, "No broker in this build (MQTT_URL); not reporting");
#endif
}

Telemetry::Status Telemetry::status() const
{
    Status s;
#ifdef MQTT_URL
    s.configured = true;
#endif
    s.connected = connected;
    s.published = published;
    s.failed = failed;
    s.commands = commands;
    strlcpy(s.deviceId, deviceId, sizeof(s.deviceId));
    return s;
}

void Telemetry::patternShown(const std::string &id)
{
    if (usageLock == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    xSemaphoreTake(usageLock, portMAX_DELAY);
    if (!currentPattern.empty())
    {
        usageMs[currentPattern] += now - currentSinceMs;
    }
    currentPattern = id;
    currentSinceMs = now;
    xSemaphoreGive(usageLock);
}

void Telemetry::settingsChanged()
{
    if (taskHandle)
    {
        xTaskNotify(taskHandle, WAKE_SETTINGS, eSetBits);
    }
}

void Telemetry::lostChanged()
{
    if (taskHandle)
    {
        xTaskNotify(taskHandle, WAKE_LOST, eSetBits);
    }
}

void Telemetry::onEvent(void *self, esp_event_base_t, int32_t, void *data)
{
    static_cast<Telemetry *>(self)->handleEvent(static_cast<esp_mqtt_event_handle_t>(data));
}

/// In the MQTT client's task: quick work only.
void Telemetry::handleEvent(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        connected = true;
        esp_mqtt_client_publish(client, (base + "/status").c_str(), "online", 0, 1, 1);
        esp_mqtt_client_subscribe(client, (base + "/set/#").c_str(), 1);
        ESP_LOGI(TAG, "Connected");
        if (taskHandle)
        {
            xTaskNotify(taskHandle, WAKE_CONNECTED, eSetBits);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (connected)
        {
            ESP_LOGW(TAG, "Disconnected");
        }
        connected = false;
        break;
    case MQTT_EVENT_PUBLISHED:
        portENTER_CRITICAL(&crashMux);
        for (int i = 0; i < crashIdCount; i++)
        {
            if (crashIds[i] == event->msg_id)
            {
                crashIds[i] = crashIds[--crashIdCount];
                crashPending = crashIdCount;
                break;
            }
        }
        portEXIT_CRITICAL(&crashMux);
        break;
    case MQTT_EVENT_DATA:
    {
        // Commands are small: ignore anything split over several events.
        if (event->current_data_offset != 0 || event->data_len != event->total_data_len)
        {
            break;
        }
        const String prefix = base + "/set/";
        const String topic(event->topic, event->topic_len);
        if (topic.startsWith(prefix))
        {
            handleCommand(topic.substring(prefix.length()).c_str(), event->data, event->data_len);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGW(TAG, "Broker refused the connection (%d): check the login", event->error_handle->connect_return_code);
        }
        break;
    default:
        break;
    }
}

void Telemetry::handleCommand(const char *name, const char *payload, size_t length)
{
    commands++;
    remote::Command cmd;
    std::string error;
    if (const char *problem = remote::parse(name, payload, length, cmd))
    {
        error = problem;
    }
    else if (cmd.id == remote::Setting::RESTART)
    {
        xTaskNotify(taskHandle, WAKE_RESTART, eSetBits);
    }
    else if (cmd.id == remote::Setting::RESEND_CRASH)
    {
        xTaskNotify(taskHandle, WAKE_RESEND_CRASH, eSetBits);
    }
    else if (handler)
    {
        error = handler(cmd);
    }
    ESP_LOGI(TAG, "set/%s: %s", name, error.empty() ? "ok" : error.c_str());

    JsonDocument doc;
    doc["setting"] = name;
    doc["ok"] = error.empty();
    if (!error.empty())
    {
        doc["error"] = error;
    }
    String body;
    serializeJson(doc, body);
    esp_mqtt_client_publish(client, (base + "/ack").c_str(), body.c_str(), body.length(), 1, 0);
    settingsChanged();
}

bool Telemetry::publish(const char *subtopic, const String &payload, int qos, bool retain, int *msgId)
{
    if (client == nullptr || !connected)
    {
        failed++;
        return false;
    }
    const int id = esp_mqtt_client_publish(client, (base + "/" + subtopic).c_str(), payload.c_str(), payload.length(), qos, retain);
    if (msgId)
    {
        *msgId = id;
    }
    if (id < 0)
    {
        failed++;
        return false;
    }
    published++;
    return true;
}

void Telemetry::task()
{
    for (;;)
    {
        uint32_t wake = 0;
        xTaskNotifyWait(0, UINT32_MAX, &wake, pdMS_TO_TICKS(1000));
        if (wake & WAKE_RESTART)
        {
            ESP_LOGW(TAG, "Restart requested over MQTT");
            settings->flush();
            vTaskDelay(pdMS_TO_TICKS(500)); // let the ack go
            ESP.restart();
        }
        if (!connected)
        {
            continue;
        }
        if (!bootSent)
        {
            // Once a boot, after the first connection.
            fetchPublicIp(true);
            publishBoot();
            publishNetwork();
            publishSettings();
            publishCrash(false);
            publishWifiScan();
            bootSent = true;
            lastReportMs = lastScanMs = millis();
            wake |= WAKE_LOST; // report lost mode at once if it is on
        }
        else if (wake & WAKE_CONNECTED)
        {
            // Back online, perhaps somewhere else: say where.
            fetchPublicIp(true);
            publishNetwork();
            if (settings->lostMode())
            {
                wake |= WAKE_LOST;
            }
        }
        if (wake & WAKE_LOST)
        {
            publishLost();
            if (settings->lostMode())
            {
                publishLocation();
            }
        }
        else if (settings->lostMode() && millis() - lastLocationMs >= LOCATION_INTERVAL_MS)
        {
            publishLocation();
        }
        if (millis() - lastScanMs >= SCAN_INTERVAL_MS)
        {
            lastScanMs = millis();
            publishWifiScan();
        }
        if (wake & WAKE_SETTINGS)
        {
            publishSettings();
        }
        if (wake & WAKE_RESEND_CRASH)
        {
            if (!publishCrash(true))
            {
                publish("ack", "{\"setting\":\"resend_crash\",\"ok\":false,\"error\":\"no core dump stored\"}", 1);
            }
        }
        if (millis() - lastReportMs >= settings->telemetryInterval() * 1000UL)
        {
            lastReportMs = millis();
            // Catches changes made on the dashboard too.
            publishSettings();
            if (settings->reportHealth())
            {
                publishHealth();
            }
            if (settings->reportUsage())
            {
                publishUsage();
            }
            if (settings->reportPerf())
            {
                publishPerf();
            }
        }
    }
}

void Telemetry::fetchPublicIp(bool force)
{
    if (publicIp.length() && !force)
    {
        return;
    }
    NetworkClientSecure tls;
    tls.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
    HTTPClient http;
    http.setTimeout(8000);
    if (http.begin(tls, "https://api.ipify.org") && http.GET() == HTTP_CODE_OK)
    {
        publicIp = http.getString();
        publicIp.trim();
    }
    else if (force)
    {
        publicIp = ""; // stale: the network may have changed
    }
    http.end();
}

void Telemetry::publishBoot()
{
    JsonDocument doc;
    const esp_app_desc_t *app = esp_app_get_description();
    doc["version"] = app->version;
    doc["fw_version"] = FW_VERSION;
#ifdef FW_TYPE
    doc["fw_type"] = FW_TYPE; // stable / beta / build, from CI
#else
    doc["fw_type"] = "DEV";
#endif
    doc["built"] = String(app->date) + " " + app->time;
    doc["idf"] = app->idf_ver;
    char sha[17];
    for (int i = 0; i < 8; i++)
    {
        snprintf(sha + i * 2, 3, "%02x", app->app_elf_sha256[i]);
    }
    doc["elf_sha256"] = sha;
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    doc["chip_revision"] = chip.revision;
    uint32_t flashSize = 0;
    esp_flash_get_size(nullptr, &flashSize);
    doc["flash_size"] = flashSize;
    doc["psram_size"] = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    doc["mac"] = WiFi.macAddress();
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    doc["ip"] = WiFi.localIP().toString();
    doc["public_ip"] = publicIp;

    doc["reset_reason"] = BootLog::reasonName(uint8_t(esp_reset_reason()));
    doc["rolled_back_from"] = bootLog->rolledBackFrom();
    JsonArray boots = doc["boots"].to<JsonArray>();
    for (int i = 0; i < BootLog::ENTRIES; i++)
    {
        const BootLog::Entry *e = bootLog->entry(i);
        if (e == nullptr)
        {
            break;
        }
        JsonObject b = boots.add<JsonObject>();
        b["version"] = e->version;
        b["started_by"] = BootLog::reasonName(e->reason);
        b["after_update"] = e->afterUpdate != 0;
        b["reached"] = BootLog::stageName(e->stage);
    }
    String body;
    serializeJson(doc, body);
    publish("boot", body, 1, true);
}

void Telemetry::publishSettings()
{
    JsonDocument doc;
    doc["pattern"] = settings->pattern();
    doc["brightness"] = settings->brightness();
    doc["ticker"] = settings->tickerText();
    doc["timezone"] = settings->timezone();
    doc["weather_location"] = settings->weatherLocation();
    doc["weather_metric"] = settings->weatherMetric();
    doc["github_updates"] = settings->github();
    doc["development"] = settings->development();
    doc["ota"] = settings->ota();
    doc["telemetry_interval"] = settings->telemetryInterval();
    doc["report_health"] = settings->reportHealth();
    doc["report_usage"] = settings->reportUsage();
    doc["report_perf"] = settings->reportPerf();
    doc["lost_mode"] = settings->lostMode();
    doc["lost_silent"] = settings->lostSilent();
    doc["lost_message"] = settings->lostMessage();
    doc["lost_pin_set"] = !settings->lostPin().empty();
    String body;
    serializeJson(doc, body);
    publish("settings", body, 1, true);
}

void Telemetry::publishHealth()
{
    JsonDocument doc;
    doc["uptime_s"] = millis() / 1000;
    doc["free_internal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    doc["min_free_internal"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    doc["largest_internal"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    doc["free_psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    doc["min_free_psram"] = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    doc["rssi"] = WiFi.RSSI();
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr(); // the access point, which can change (roaming, mesh)
    doc["channel"] = WiFi.channel();
    doc["pattern"] = settings->pattern();
    doc["published"] = published;
    doc["failed"] = failed;
    String body;
    serializeJson(doc, body);
    publish("health", body);
}

void Telemetry::publishUsage()
{
    JsonDocument doc;
    const uint32_t now = millis();
    xSemaphoreTake(usageLock, portMAX_DELAY);
    if (!currentPattern.empty())
    {
        usageMs[currentPattern] += now - currentSinceMs;
        currentSinceMs = now;
    }
    JsonObject patterns = doc["patterns_s"].to<JsonObject>();
    for (const auto &p : usageMs)
    {
        patterns[p.first] = p.second / 1000;
    }
    usageMs.clear();
    doc["period_s"] = (now - usageStartMs) / 1000;
    usageStartMs = now;
    xSemaphoreGive(usageLock);

    doc["brightness"] = settings->brightness();
    doc["spotify_linked"] = Spotify::account().linked;
    doc["weather_location_set"] = !settings->weatherLocation().empty();
    doc["ticker_set"] = !settings->tickerText().empty();
    String body;
    serializeJson(doc, body);
    publish("usage", body);
}

/// Scans (blocking, in this task: up to ~120 ms a channel, staying
/// connected) and adds up to `max` networks, strongest first. Returns how many
/// were found, or a negative error.
int Telemetry::scanInto(JsonArray nets, int max)
{
    const int16_t found = WiFi.scanNetworks(false, true, false, 120);
    if (found < 0)
    {
        ESP_LOGW(TAG, "WiFi scan failed (%d)", found);
        return found;
    }
    static const char *const AUTH[] = {"open", "wep", "wpa", "wpa2", "wpa/wpa2", "wpa2-enterprise", "wpa3", "wpa2/wpa3",
                                       "wapi", "owe", "wpa3-enterprise-192", "wpa3-ext-psk", "wpa3-ext-psk-mixed",
                                       "dpp", "wpa3-enterprise", "wpa2/wpa3-enterprise"};
    for (int i = 0; i < found && i < max; i++)
    {
        JsonObject n = nets.add<JsonObject>();
        n["ssid"] = WiFi.SSID(i);
        n["bssid"] = WiFi.BSSIDstr(i);
        n["rssi"] = WiFi.RSSI(i);
        n["channel"] = WiFi.channel(i);
        const unsigned auth = unsigned(WiFi.encryptionType(i));
        n["auth"] = auth < sizeof(AUTH) / sizeof(AUTH[0]) ? AUTH[auth] : "other";
    }
    WiFi.scanDelete();
    return found;
}

/// The networks in range: names, access points, signal, channel, security.
void Telemetry::publishWifiScan()
{
    const uint32_t start = millis();
    JsonDocument doc;
    doc["connected_ssid"] = WiFi.SSID();
    doc["connected_bssid"] = WiFi.BSSIDstr();
    const int found = scanInto(doc["networks"].to<JsonArray>(), MAX_SCAN_RESULTS);
    if (found < 0)
    {
        return;
    }
    doc["count"] = found;
    doc["scan_ms"] = millis() - start;
    String body;
    serializeJson(doc, body);
    publish("wifi", body, 0, true);
}

/// Where the cube is online now. Sent on every (re)connection.
void Telemetry::publishNetwork()
{
    JsonDocument doc;
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    doc["ip"] = WiFi.localIP().toString();
    doc["public_ip"] = publicIp;
    doc["via"] = lostMode ? lostMode->via() : "saved";
    doc["uptime_s"] = millis() / 1000;
    time_t now = time(nullptr);
    if (now > 1700000000)
    {
        doc["time"] = (long long)now;
    }
    String body;
    serializeJson(doc, body);
    publish("network", body, 1, true);
}

/// While lost: everything that could place the cube. A fresh public IP, the
/// network and access point it is on, and what else is in range (nearby
/// access points can be geolocated).
void Telemetry::publishLocation()
{
    lastLocationMs = millis();
    fetchPublicIp(true);
    JsonDocument doc;
    doc["lost"] = settings->lostMode();
    doc["public_ip"] = publicIp;
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    doc["ip"] = WiFi.localIP().toString();
    doc["via"] = lostMode ? lostMode->via() : "saved";
    doc["uptime_s"] = millis() / 1000;
    time_t now = time(nullptr);
    if (now > 1700000000)
    {
        doc["time"] = (long long)now;
    }
    scanInto(doc["networks"].to<JsonArray>(), 15);
    String body;
    serializeJson(doc, body);
    publish("location", body, 1, true);
}

void Telemetry::publishLost()
{
    JsonDocument doc;
    doc["active"] = settings->lostMode();
    doc["silent"] = settings->lostSilent();
    doc["message"] = settings->lostMessage();
    doc["pin_set"] = !settings->lostPin().empty();
    doc["uptime_s"] = millis() / 1000;
    time_t now = time(nullptr);
    if (now > 1700000000)
    {
        doc["time"] = (long long)now;
    }
    String body;
    serializeJson(doc, body);
    publish("lost", body, 1, true);
}

void Telemetry::publishPerf()
{
    const Renderer::Stats s = renderer->stats();
    JsonDocument doc;
    doc["pattern"] = s.pattern;
    doc["fps"] = s.windowMs ? s.frames * 1000.0f / s.windowMs : 0.0f;
    doc["tick_avg_us"] = s.tickAvgUs;
    doc["tick_max_us"] = s.tickMaxUs;
    doc["push_avg_us"] = s.pushAvgUs;
    doc["push_max_us"] = s.pushMaxUs;
    String body;
    serializeJson(doc, body);
    publish("perf", body);
}

/**
 * The stored core dump, if there is one and it has not been sent: a summary
 * on crash, then the raw dump, base64, on crash/<n>. Remembered as sent (by
 * CRC) only once the broker has acknowledged every part.
 */
bool Telemetry::publishCrash(bool force)
{
    if (esp_core_dump_image_check() != ESP_OK)
    {
        ESP_LOGI(TAG, "No core dump stored");
        return false;
    }
    size_t addr = 0, size = 0;
    if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0)
    {
        ESP_LOGW(TAG, "Could not locate the core dump");
        return false;
    }
    const int chunks = int((size + CRASH_CHUNK - 1) / CRASH_CHUNK);
    if (chunks > MAX_CRASH_CHUNKS)
    {
        ESP_LOGW(TAG, "Core dump too big to send (%u bytes)", (unsigned)size);
        return false;
    }
    uint8_t *raw = static_cast<uint8_t *>(heap_caps_malloc(CRASH_CHUNK, MALLOC_CAP_SPIRAM));
    if (raw == nullptr)
    {
        return false;
    }
    uint32_t crc = 0;
    for (size_t off = 0; off < size; off += CRASH_CHUNK)
    {
        const size_t n = min(CRASH_CHUNK, size - off);
        esp_flash_read(nullptr, raw, addr + off, n);
        crc = esp_rom_crc32_le(crc, raw, n);
    }
    if (!force && prefs.getUInt("crashSent", 0) == crc)
    {
        ESP_LOGI(TAG, "Core dump %s already sent", hex32(crc).c_str());
        free(raw);
        return false;
    }

    // The summary: enough to see what crashed without the dump.
    esp_core_dump_summary_t *summary = static_cast<esp_core_dump_summary_t *>(heap_caps_calloc(1, sizeof(esp_core_dump_summary_t), MALLOC_CAP_SPIRAM));
    JsonDocument doc;
    doc["crc"] = hex32(crc);
    doc["size"] = size;
    doc["chunks"] = chunks;
    if (summary && esp_core_dump_get_summary(summary) == ESP_OK)
    {
        doc["task"] = summary->exc_task;
        doc["pc"] = hex32(summary->exc_pc);
        doc["cause"] = summary->ex_info.exc_cause;
        doc["vaddr"] = hex32(summary->ex_info.exc_vaddr);
        doc["elf_sha256"] = summary->app_elf_sha256;
        JsonArray bt = doc["backtrace"].to<JsonArray>();
        for (uint32_t i = 0; i < summary->exc_bt_info.depth && i < 16; i++)
        {
            bt.add(hex32(summary->exc_bt_info.bt[i]));
        }
        doc["backtrace_corrupted"] = summary->exc_bt_info.corrupted;
    }
    free(summary);

    portENTER_CRITICAL(&crashMux);
    crashIdCount = 0;
    portEXIT_CRITICAL(&crashMux);
    auto track = [](int id)
    {
        if (id > 0)
        {
            portENTER_CRITICAL(&crashMux);
            if (crashIdCount < MAX_CRASH_CHUNKS + 1)
            {
                crashIds[crashIdCount++] = id;
            }
            portEXIT_CRITICAL(&crashMux);
        }
    };
    String body;
    serializeJson(doc, body);
    int id = -1;
    bool ok = publish("crash", body, 1, false, &id);
    track(id);

    // Base64 of one chunk: 4 * ceil(3072 / 3) + 1.
    const size_t encodedSize = 4 * ((CRASH_CHUNK + 2) / 3) + 1;
    unsigned char *encoded = static_cast<unsigned char *>(heap_caps_malloc(encodedSize, MALLOC_CAP_SPIRAM));
    for (int i = 0; ok && encoded && i < chunks; i++)
    {
        const size_t off = size_t(i) * CRASH_CHUNK;
        const size_t n = min(CRASH_CHUNK, size - off);
        esp_flash_read(nullptr, raw, addr + off, n);
        size_t written = 0;
        mbedtls_base64_encode(encoded, encodedSize, &written, raw, n);
        String part = String("{\"crc\":\"") + hex32(crc) + "\",\"n\":" + i + ",\"of\":" + chunks + ",\"data\":\"" +
                      reinterpret_cast<const char *>(encoded) + "\"}";
        ok = publish((String("crash/") + i).c_str(), part, 1, false, &id);
        track(id);
    }
    free(encoded);
    free(raw);

    // Remember it as sent only once the broker has every part.
    portENTER_CRITICAL(&crashMux);
    crashPending = crashIdCount;
    portEXIT_CRITICAL(&crashMux);
    const uint32_t start = millis();
    while (ok && crashPending > 0 && millis() - start < 30000)
    {
        portENTER_CRITICAL(&crashMux);
        crashPending = crashIdCount;
        portEXIT_CRITICAL(&crashMux);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    portENTER_CRITICAL(&crashMux);
    const bool allAcked = crashIdCount == 0;
    portEXIT_CRITICAL(&crashMux);
    if (ok && allAcked)
    {
        prefs.putUInt("crashSent", crc);
        ESP_LOGI(TAG, "Sent core dump %s (%u bytes, %d parts)", hex32(crc).c_str(), (unsigned)size, chunks);
    }
    return true;
}
