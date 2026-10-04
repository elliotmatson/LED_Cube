#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <functional>
#include <Arduino.h>
#include <ESPDashPro.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "renderer.h"

/**
 * What the cube is doing and how it is connected, on the dashboard's
 * Statistics page: WiFi network, local and public IP, memory, uptime, the
 * last reset, the pattern showing and its frame rate, chip temperature.
 *
 * Most values are StatisticProviders, read whenever the dashboard sends an
 * update. A low-priority task sends one every few seconds so they stay
 * current while the page is open, logs memory to the serial port, and looks
 * up the public IP address (which needs a request to an outside service) in
 * the background, so nothing on the dashboard or the REST API waits for it.
 */
class Analytics
{
public:
    /// Adds the statistics to `dashboard`, after any already there.
    explicit Analytics(ESPDash &dashboard);

    /**
     * Fills in the values that never change and starts the task.
     *
     * @param renderer    for the frame rate.
     * @param patternName the name of the pattern showing (or selected).
     */
    void begin(Renderer &renderer, std::function<String()> patternName);

    /// The public IP address from the last successful lookup, or "" before
    /// the first. Safe from any task.
    String publicIp();

private:
    void loop();
    bool lookUpPublicIp();

    ESPDash &dashboard;
    Renderer *renderer = nullptr;
    std::function<String()> patternName;

    dash::StatisticProvider<dash::string> network;
    dash::StatisticProvider<dash::string> accessPoint;
    dash::StatisticProvider<dash::string> localIp;
    dash::StatisticProvider<dash::string> publicIpStat;
    dash::StatisticValue<dash::string> macAddress;
    dash::StatisticProvider<dash::string> uptime;
    dash::StatisticValue<dash::string> lastReset;
    dash::StatisticProvider<dash::string> localTime;
    dash::StatisticProvider<dash::string> pattern;
    dash::StatisticProvider<float, 1> frameRate;
    dash::StatisticProvider<dash::string> freeInternal;
    dash::StatisticProvider<dash::string> minFreeInternal;
    dash::StatisticProvider<dash::string> largestInternal;
    dash::StatisticProvider<dash::string> freePsram;
    dash::StatisticProvider<float, 1> chipTemperature;

    // Written by the task, read from the dashboard and the REST API.
    char publicIpText[46] = "";
    portMUX_TYPE publicIpMux = portMUX_INITIALIZER_UNLOCKED;

    TaskHandle_t task = nullptr;
};

#endif
