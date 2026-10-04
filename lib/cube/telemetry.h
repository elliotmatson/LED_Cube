#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mqtt_client.h>
#include <functional>
#include <map>
#include <string>

#include "boot_log.h"
#include "remote.h"
#include "renderer.h"
#include "settings.h"

/**
 * Reports from the cube to an MQTT broker, and settings from the broker to
 * the cube. Off unless the build defines MQTT_URL (with MQTT_USER and
 * MQTT_PASSWORD) in secrets.h: CI writes it from repository secrets.
 *
 * Every cube publishes under cube/<id>/, where <id> is random, made once
 * and kept in NVS, and is also its MQTT client ID -- so the broker can limit
 * the shared login to a cube's own topics:
 *
 *   status    online / offline (retained; offline is the last will)
 *   boot      once a boot: version, boot log, network, public IP
 *   health    every interval: uptime, memory low-water marks, signal
 *   usage     every interval: time per pattern, brightness, features in use
 *   perf      every interval: the renderer's frame times
 *   wifi      at boot and hourly: the networks in range (a scan)
 *   crash     once per new core dump: a summary, then crash/<n> chunks
 *   settings  current settings (retained)
 *   ack       the result of each command
 *
 * and reads cube/<id>/set/<name> (see lib/remote for names and values).
 * The shared login should be allowed to publish only those topics and read
 * only set/: commands come from a separate admin login.
 */
class Telemetry
{
public:
    /// Applies a setting; returns "" on success or why it failed.
    using CommandHandler = std::function<std::string(const remote::Command &)>;

    void begin(Settings &settings, BootLog &bootLog, Renderer &renderer, CommandHandler handler);

    /// The pattern now showing, for usage reports. Any task.
    void patternShown(const std::string &id);
    /// Settings changed: republish them.
    void settingsChanged();

    struct Status
    {
        bool configured = false; // the build has a broker
        bool connected = false;
        uint32_t published = 0, failed = 0, commands = 0;
        char deviceId[17] = "";
    };
    Status status() const;

private:
    static void onEvent(void *self, esp_event_base_t base, int32_t id, void *data);
    void handleEvent(esp_mqtt_event_handle_t event);
    void handleCommand(const char *name, const char *payload, size_t length);
    void task();

    bool publish(const char *subtopic, const String &payload, int qos = 0, bool retain = false, int *msgId = nullptr);
    void publishBoot();
    void publishSettings();
    void publishHealth();
    void publishUsage();
    void publishPerf();
    void publishWifiScan();
    bool publishCrash(bool force);
    void fetchPublicIp();

    Settings *settings = nullptr;
    BootLog *bootLog = nullptr;
    Renderer *renderer = nullptr;
    CommandHandler handler;
    Preferences prefs;

    esp_mqtt_client_handle_t client = nullptr;
    TaskHandle_t taskHandle = nullptr;
    volatile bool connected = false;
    bool bootSent = false;
    char deviceId[17] = "";
    String base; // "cube/<id>"
    String publicIp;
    uint32_t lastReportMs = 0;
    uint32_t lastScanMs = 0;
    volatile uint32_t published = 0, failed = 0, commands = 0;

    // Crash upload: waits for the broker to acknowledge every chunk.
    volatile int crashPending = 0;

    // Usage: milliseconds per pattern since the last report.
    SemaphoreHandle_t usageLock = nullptr;
    std::map<std::string, uint32_t> usageMs;
    std::string currentPattern;
    uint32_t currentSinceMs = 0;
    uint32_t usageStartMs = 0;
};

#endif
