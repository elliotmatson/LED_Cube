#include "settings.h"

// NVS keys (15 characters at most).
static const char *K_BRIGHTNESS = "brightness";
static const char *K_DEVELOPMENT = "development";
static const char *K_OTA = "ota";
static const char *K_GITHUB = "github";
static const char *K_SIGNED = "signedOnly";
static const char *K_LATCH = "latch";
static const char *K_20MHZ = "use20MHz";
static const char *K_PATTERN = "pattern";
static const char *K_TIMEZONE = "timezone";
static const char *K_TICKER = "ticker";
static const char *K_WEATHER_LOCATION = "wxLocation";
static const char *K_WEATHER_METRIC = "wxMetric";
static const char *K_LEGACY = "cubePrefs";

static const uint64_t FLUSH_DELAY_US = 2 * 1000 * 1000;

static uint8_t clampLatch(int value) { return value < 1 ? 1 : (value > 4 ? 4 : value); }

bool Settings::begin()
{
    _lock = xSemaphoreCreateMutex();
    esp_timer_create_args_t timer = {};
    timer.callback = [](void *self)
    { static_cast<Settings *>(self)->flush(); };
    timer.arg = this;
    timer.name = "settings";
    esp_timer_create(&timer, &_flushTimer);

    if (!_prefs.begin("cube"))
    {
        log("NVS unavailable, using defaults");
        return false;
    }
    migrateLegacyBlob();

    _brightness = _prefs.getUChar(K_BRIGHTNESS, _brightness);
    _development = _prefs.getBool(K_DEVELOPMENT, _development);
    _ota = _prefs.getBool(K_OTA, _ota);
    _github = _prefs.getBool(K_GITHUB, _github);
    _signedFirmwareOnly = _prefs.getBool(K_SIGNED, _signedFirmwareOnly);
    _latchBlanking = clampLatch(_prefs.getUChar(K_LATCH, _latchBlanking));
    _use20MHz = _prefs.getBool(K_20MHZ, _use20MHz);
    _pattern = _prefs.getString(K_PATTERN, "").c_str();
    _timezone = _prefs.getString(K_TIMEZONE, "").c_str();
    _tickerText = _prefs.getString(K_TICKER, "").c_str();
    _weatherLocation = _prefs.getString(K_WEATHER_LOCATION, "").c_str();
    _weatherMetric = _prefs.getBool(K_WEATHER_METRIC, _weatherMetric);
    if (_prefs.isKey("HW"))
    {
        _hardware = _prefs.getString("HW").c_str();
    }
    log("Loaded settings");
    return true;
}

// Firmware before this stored everything as one struct with putBytes(). Its
// layout, which must not change: this is only ever read.
struct LegacyCubePrefs
{
    uint8_t brightness;
    bool development;
    bool ota;
    bool github;
    bool signedFWOnly;
    uint8_t latchBlanking;
    bool use20MHz;
    uint8_t patternIndex;
};

void Settings::migrateLegacyBlob()
{
    if (!_prefs.isKey(K_LEGACY))
    {
        return;
    }
    LegacyCubePrefs old;
    if (_prefs.getBytesLength(K_LEGACY) == sizeof(old) && _prefs.getBytes(K_LEGACY, &old, sizeof(old)) == sizeof(old))
    {
        _prefs.putUChar(K_BRIGHTNESS, old.brightness);
        _prefs.putBool(K_DEVELOPMENT, old.development);
        _prefs.putBool(K_OTA, old.ota);
        _prefs.putBool(K_GITHUB, old.github);
        _prefs.putBool(K_SIGNED, old.signedFWOnly);
        _prefs.putUChar(K_LATCH, clampLatch(old.latchBlanking));
        _prefs.putBool(K_20MHZ, old.use20MHz);
        _legacyPatternIndex = old.patternIndex;
        ESP_LOGI("Settings", "Imported settings saved by older firmware");
    }
    _prefs.remove(K_LEGACY);
}

void Settings::changed(Key key)
{
    _dirty |= key;
    if (_flushTimer)
    {
        esp_timer_stop(_flushTimer); // restart the delay on every change
        esp_timer_start_once(_flushTimer, FLUSH_DELAY_US);
    }
}

#define SETTER(name, field, key, expr)              \
    void Settings::name                             \
    {                                               \
        xSemaphoreTake(_lock, portMAX_DELAY);       \
        auto next = (expr);                         \
        if (field != next)                          \
        {                                           \
            field = next;                           \
            changed(key);                           \
        }                                           \
        xSemaphoreGive(_lock);                      \
    }

SETTER(setBrightness(uint8_t value), _brightness, BRIGHTNESS, value)
SETTER(setDevelopment(bool value), _development, DEVELOPMENT, value)
SETTER(setOta(bool value), _ota, OTA, value)
SETTER(setGithub(bool value), _github, GITHUB, value)
SETTER(setSignedFirmwareOnly(bool value), _signedFirmwareOnly, SIGNED, value)
SETTER(setLatchBlanking(uint8_t value), _latchBlanking, LATCH, clampLatch(value))
SETTER(setUse20MHz(bool value), _use20MHz, CLOCK_20MHZ, value)
SETTER(setPattern(const std::string &id), _pattern, PATTERN, id)
SETTER(setTimezone(const std::string &name), _timezone, TIMEZONE, name)
SETTER(setTickerText(const std::string &text), _tickerText, TICKER, text)
SETTER(setWeatherLocation(const std::string &text), _weatherLocation, WEATHER_LOCATION, text)
SETTER(setWeatherMetric(bool value), _weatherMetric, WEATHER_METRIC, value)

#undef SETTER

std::string Settings::pattern() const
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    std::string copy = _pattern;
    xSemaphoreGive(_lock);
    return copy;
}

std::string Settings::tickerText() const
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    std::string copy = _tickerText;
    xSemaphoreGive(_lock);
    return copy;
}

std::string Settings::weatherLocation() const
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    std::string copy = _weatherLocation;
    xSemaphoreGive(_lock);
    return copy;
}

std::string Settings::timezone() const
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    std::string copy = _timezone;
    xSemaphoreGive(_lock);
    return copy;
}

void Settings::flush()
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    uint16_t dirty = _dirty;
    _dirty = 0;
    if (dirty & BRIGHTNESS)
        _prefs.putUChar(K_BRIGHTNESS, _brightness);
    if (dirty & DEVELOPMENT)
        _prefs.putBool(K_DEVELOPMENT, _development);
    if (dirty & OTA)
        _prefs.putBool(K_OTA, _ota);
    if (dirty & GITHUB)
        _prefs.putBool(K_GITHUB, _github);
    if (dirty & SIGNED)
        _prefs.putBool(K_SIGNED, _signedFirmwareOnly);
    if (dirty & LATCH)
        _prefs.putUChar(K_LATCH, _latchBlanking);
    if (dirty & CLOCK_20MHZ)
        _prefs.putBool(K_20MHZ, _use20MHz);
    if (dirty & PATTERN)
        _prefs.putString(K_PATTERN, _pattern.c_str());
    if (dirty & TIMEZONE)
        _prefs.putString(K_TIMEZONE, _timezone.c_str());
    if (dirty & TICKER)
        _prefs.putString(K_TICKER, _tickerText.c_str());
    if (dirty & WEATHER_LOCATION)
        _prefs.putString(K_WEATHER_LOCATION, _weatherLocation.c_str());
    if (dirty & WEATHER_METRIC)
        _prefs.putBool(K_WEATHER_METRIC, _weatherMetric);
    if (dirty)
    {
        log("Saved settings");
    }
    xSemaphoreGive(_lock);
}

void Settings::log(const char *prefix) const
{
    ESP_LOGI("Settings", "%s: brightness %u, development %d, ota %d, github %d, signed only %d, latch %u, 20 MHz %d, pattern '%s', timezone '%s'",
             prefix, _brightness, _development, _ota, _github, _signedFirmwareOnly, _latchBlanking, _use20MHz,
             _pattern.c_str(), _timezone.c_str());
}
