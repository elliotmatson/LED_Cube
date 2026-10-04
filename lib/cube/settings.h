#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>

/**
 * The cube's persistent settings, one NVS key each in namespace "cube".
 *
 * Replaces a raw putBytes() of a struct, which was validated only by its
 * size: a reordered struct read garbage, and nothing was range-checked.
 * Values are clamped on load. Setters update memory at once and write NVS
 * two seconds after the last change, so dragging a slider is one flash write
 * rather than dozens. Call flush() before a deliberate restart.
 *
 * Safe to use from any task.
 */
class Settings
{
public:
    /// Opens NVS, imports the old "cubePrefs" blob if present, and loads.
    bool begin();

    uint8_t brightness() const { return _brightness; }
    void setBrightness(uint8_t value);

    bool development() const { return _development; }
    void setDevelopment(bool value);

    bool ota() const { return _ota; }
    void setOta(bool value);

    bool github() const { return _github; }
    void setGithub(bool value);

    bool signedFirmwareOnly() const { return _signedFirmwareOnly; }
    void setSignedFirmwareOnly(bool value);

    /// HUB75 latch blanking, 1-4. Takes effect after a restart.
    uint8_t latchBlanking() const { return _latchBlanking; }
    void setLatchBlanking(uint8_t value);

    /// 20 MHz HUB75 clock instead of 10. Takes effect after a restart.
    bool use20MHz() const { return _use20MHz; }
    void setUse20MHz(bool value);

    /// The selected pattern's id (Pattern::id()), or empty for the default.
    std::string pattern() const;
    void setPattern(const std::string &id);

    /// Time zone name from lib/timezones, or empty for the default.
    std::string timezone() const;
    void setTimezone(const std::string &name);

    /// The Ticker pattern's custom message, or empty.
    std::string tickerText() const;
    void setTickerText(const std::string &text);

    /// Where the Weather pattern reports from: a place name or postcode, or
    /// empty to locate the cube by its IP address.
    std::string weatherLocation() const;
    void setWeatherLocation(const std::string &text);

    /// Weather in degrees C and km/h rather than F and mph.
    bool weatherMetric() const { return _weatherMetric; }
    void setWeatherMetric(bool value);

    /// Hardware revision written at manufacture (key "HW"), or empty.
    std::string hardware() const { return _hardware; }

    /// Pattern position saved by firmware that stored an index rather than
    /// an id, or -1. Only meaningful right after begin() migrated it.
    int legacyPatternIndex() const { return _legacyPatternIndex; }

    /// Writes pending changes now.
    void flush();

    void log(const char *prefix) const;

private:
    enum Key : uint16_t
    {
        BRIGHTNESS = 1 << 0,
        DEVELOPMENT = 1 << 1,
        OTA = 1 << 2,
        GITHUB = 1 << 3,
        SIGNED = 1 << 4,
        LATCH = 1 << 5,
        CLOCK_20MHZ = 1 << 6,
        PATTERN = 1 << 7,
        TIMEZONE = 1 << 8,
        TICKER = 1 << 9,
        WEATHER_LOCATION = 1 << 10,
        WEATHER_METRIC = 1 << 11,
    };

    void changed(Key key);
    void migrateLegacyBlob();

    Preferences _prefs;
    SemaphoreHandle_t _lock = nullptr;
    esp_timer_handle_t _flushTimer = nullptr;
    uint16_t _dirty = 0;

    uint8_t _brightness = 255;
    bool _development = false;
    bool _ota = false;
    bool _github = true;
    bool _signedFirmwareOnly = true;
    uint8_t _latchBlanking = 1;
    bool _use20MHz = false;
    std::string _pattern;
    std::string _timezone;
    std::string _tickerText;
    std::string _weatherLocation;
    bool _weatherMetric = false;
    std::string _hardware;
    int _legacyPatternIndex = -1;
};

#endif
