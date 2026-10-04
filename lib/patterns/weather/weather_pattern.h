#ifndef WEATHER_PATTERN_H
#define WEATHER_PATTERN_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include "cube_utils.h"
#include "color.h"
#include "weather.h"

/**
 * The weather outside. The side faces (one 128-pixel strip, BottomPanels)
 * show the sky for the current conditions, by day or night: sun and
 * turning rays or moon and stars, drifting clouds, fog, drizzle, rain, snow,
 * or a storm with lightning. The top face shows the temperature, the
 * conditions and today's high and low.
 *
 * A worker task, running while the pattern does (as Spotify's), finds the
 * cube's location -- by geocoding the place set on the dashboard, or from
 * its IP address -- and fetches the forecast from Open-Meteo every 15
 * minutes. Both are kept between runs, so coming back is instant.
 */
class WeatherPattern : public Pattern
{
public:
    WeatherPattern();
    ~WeatherPattern();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    /// The place to report from: a name or postcode, or empty for the IP
    /// address's location. From the dashboard or the API; any task.
    static void setLocation(const std::string &text);
    /// Degrees C and km/h (true) or F and mph. Any task.
    static void setMetric(bool metric);
    /// Shows weather code `wmo` (by day or night) for two minutes, whatever
    /// the real weather is: for trying the animations. -1 stops it.
    static void preview(int wmo, bool day);

    /// What the pattern knows, for the dashboard and the API.
    struct Report
    {
        bool located = false;  // a place has been found
        char place[48] = "";   // "Austin, Texas"
        bool fromIp = false;   // found from the IP address
        bool current = false;  // a forecast has been fetched
        float tempC = 0, highC = 0, lowC = 0, windKmh = 0;
        int code = 0;
        bool day = true;
        uint32_t ageS = 0;     // since the forecast was fetched
        bool metric = false;
        char error[64] = "";   // the last problem, if any
        bool preview = false;  // code and day are a preview
    };
    static Report report();

    static const int DROPS = 160;
    static const int CLOUDS = 8;
    static const int STARS = 40;

private:
    struct Drop
    {
        float x, y, speed, phase;
        uint8_t size;
    };
    struct Cloud
    {
        float x, y, scale, speed;
    };

    void worker();
    bool locate(const std::string &query);
    bool fetchForecast();

    float frand();
    void setScene(weather::Sky sky, float intensity);
    void respawnDrop(Drop &d, bool anywhere);
    void drawSides(const Report &r, float t, float dt);
    void drawTop(const Report &r);
    void newBolt();

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;

    TaskHandle_t workerTask = nullptr;
    SemaphoreHandle_t workerDone = nullptr;
    volatile bool running = false;

    Drop drops[DROPS];
    int dropCount = 0;
    Cloud clouds[CLOUDS];
    int cloudCount = 0;
    uint16_t stars[STARS]; // strip pixel index: y * 128 + x
    weather::Sky scene = weather::Sky::CLEAR;
    bool sceneSet = false;
    float sceneIntensity = 0;

    // Lightning: when the next strike comes, and the current bolt.
    uint32_t nextStrikeMs = 0, strikeMs = 0;
    int8_t boltX[16];
    int boltPoints = 0;

    uint32_t topKey = 0; // what the top face shows, to redraw only on change
    uint32_t startMs = 0, lastMs = 0;
    uint32_t rng = 1;
};

#endif
