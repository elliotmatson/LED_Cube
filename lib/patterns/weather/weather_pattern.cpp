#include "weather_pattern.h"
#include "fonts.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <math.h>

// ESP-IDF's certificate bundle (see updates.h).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace
{
    const uint32_t REFRESH_MS = 15 * 60 * 1000;
    const uint32_t RETRY_MS = 60 * 1000;
    const uint32_t PREVIEW_MS = 2 * 60 * 1000;
    const int STRIP_W = 2 * cube::FACE_SIZE;
    const int STRIP_H = cube::FACE_SIZE;

    // Shared between the worker, tick() and the dashboard / API.
    SemaphoreHandle_t stateLock()
    {
        static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        return lock;
    }
    struct Shared
    {
        std::string location; // as set
        bool metric = false;
        // The place found, and for which setting.
        bool located = false;
        std::string locatedFor;
        float lat = 0, lon = 0;
        std::string place;
        bool fromIp = false;
        // The forecast.
        bool current = false;
        uint32_t fetchedMs = 0;
        float tempC = 0, highC = 0, lowC = 0, windKmh = 0;
        int code = 0;
        bool day = true;
        std::string error;
        TaskHandle_t worker = nullptr; // the running pattern's, to wake it
        int previewCode = -1;
        bool previewDay = true;
        uint32_t previewMs = 0;
    } shared;

    struct Lock
    {
        Lock() { xSemaphoreTake(stateLock(), portMAX_DELAY); }
        ~Lock() { xSemaphoreGive(stateLock()); }
    };

    void setError(const std::string &e)
    {
        Lock l;
        shared.error = e;
        if (!e.empty())
        {
            ESP_LOGW("Weather", "%s", e.c_str());
        }
    }

    /// GETs `url` over HTTPS and parses the parts `filter` keeps.
    bool getJson(const String &url, JsonDocument &filter, JsonDocument &doc)
    {
        NetworkClientSecure client;
        client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
        HTTPClient http;
        http.useHTTP10(true);
        http.setTimeout(10000);
        http.setUserAgent("LED-Cube");
        if (!http.begin(client, url))
        {
            return false;
        }
        const int code = http.GET();
        if (code != HTTP_CODE_OK)
        {
            ESP_LOGW("Weather", "%s: HTTP %d", url.c_str(), code);
            http.end();
            return false;
        }
        const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        http.end();
        if (err)
        {
            ESP_LOGW("Weather", "%s: %s", url.c_str(), err.c_str());
            return false;
        }
        return true;
    }

    // Sky colours, top of the strip to the bottom, by day and by night.
    struct SkyColors
    {
        color::RGB dayTop, dayBottom, nightTop, nightBottom, dayCloud, nightCloud;
    };
    const SkyColors SKIES[] = {
        /* CLEAR */ {{20, 90, 210}, {110, 170, 240}, {2, 3, 16}, {10, 14, 42}, {240, 242, 248}, {50, 52, 62}},
        /* PARTLY */ {{25, 95, 205}, {115, 170, 235}, {3, 4, 18}, {12, 15, 42}, {240, 242, 248}, {55, 57, 68}},
        /* CLOUDY */ {{70, 80, 96}, {120, 126, 136}, {12, 13, 19}, {25, 27, 34}, {175, 180, 190}, {45, 47, 55}},
        /* FOG */ {{95, 100, 108}, {135, 138, 142}, {22, 23, 28}, {38, 39, 44}, {200, 202, 206}, {60, 61, 66}},
        /* DRIZZLE */ {{62, 70, 84}, {98, 104, 114}, {10, 11, 17}, {21, 23, 30}, {125, 130, 140}, {38, 40, 48}},
        /* RAIN */ {{50, 57, 70}, {86, 92, 102}, {9, 10, 15}, {19, 21, 27}, {105, 110, 120}, {32, 34, 42}},
        /* SNOW */ {{110, 116, 128}, {170, 176, 186}, {24, 26, 34}, {44, 47, 58}, {200, 204, 212}, {62, 64, 74}},
        /* STORM */ {{28, 30, 40}, {54, 57, 66}, {7, 7, 13}, {15, 15, 23}, {62, 64, 74}, {24, 25, 32}},
    };

    // A cloud is a few overlapping puffs: offsets and radii at scale 1.
    struct Puff
    {
        float dx, dy, r;
    };
    const Puff PUFFS[] = {{0, 0, 7}, {-8, 2, 5}, {8, 2, 5.5f}, {-4, -3, 5}, {4, -4, 5.5f}};
}

WeatherPattern::WeatherPattern()
{
    data.id = "weather";
    data.name = "Weather";
}

WeatherPattern::~WeatherPattern()
{
    end();
}

void WeatherPattern::setLocation(const std::string &text)
{
    Lock l;
    shared.location = text;
    if (shared.worker)
    {
        xTaskNotifyGive(shared.worker);
    }
}

void WeatherPattern::setMetric(bool metric)
{
    Lock l;
    shared.metric = metric;
}

void WeatherPattern::preview(int wmo, bool day)
{
    Lock l;
    shared.previewCode = wmo;
    shared.previewDay = day;
    shared.previewMs = millis();
}

WeatherPattern::Report WeatherPattern::report()
{
    Lock l;
    Report r;
    // Only a place found for the current setting counts.
    r.located = shared.located && shared.locatedFor == shared.location;
    strlcpy(r.place, shared.place.c_str(), sizeof(r.place));
    r.fromIp = shared.fromIp;
    r.current = r.located && shared.current;
    r.tempC = shared.tempC;
    r.highC = shared.highC;
    r.lowC = shared.lowC;
    r.windKmh = shared.windKmh;
    r.code = shared.code;
    r.day = shared.day;
    r.ageS = shared.current ? (millis() - shared.fetchedMs) / 1000 : 0;
    r.metric = shared.metric;
    strlcpy(r.error, shared.error.c_str(), sizeof(r.error));
    if (shared.previewCode >= 0 && millis() - shared.previewMs < PREVIEW_MS)
    {
        r.preview = true;
        r.code = shared.previewCode;
        r.day = shared.previewDay;
        if (!r.current)
        {
            // Something to show while nothing has been fetched.
            r.current = true;
            r.tempC = 20;
            r.highC = 24;
            r.lowC = 14;
        }
    }
    return r;
}

float WeatherPattern::frand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xFFFF) / 65535.0f;
}

void WeatherPattern::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    rng = esp_random() | 1;
    for (int i = 0; i < STARS; i++)
    {
        stars[i] = uint16_t((esp_random() % 40) * STRIP_W + esp_random() % STRIP_W);
    }
    sceneSet = false;
    topKey = 0;
    startMs = lastMs = millis();
    nextStrikeMs = startMs + 2000;
    strikeMs = 0;

    if (workerDone == nullptr)
    {
        workerDone = xSemaphoreCreateBinary();
    }
    running = true;
    xTaskCreate(
        [](void *self)
        { static_cast<WeatherPattern *>(self)->worker(); },
        "Weather", 10240, this, 1, &workerTask);
    Lock l;
    shared.worker = workerTask;
}

void WeatherPattern::end()
{
    if (workerTask)
    {
        {
            Lock l;
            shared.worker = nullptr;
        }
        running = false;
        xTaskNotifyGive(workerTask);
        // A request in flight has to finish or time out first.
        if (xSemaphoreTake(workerDone, pdMS_TO_TICKS(25000)) != pdTRUE)
        {
            ESP_LOGE("Weather", "Worker did not stop; deleting it");
            vTaskDelete(workerTask);
        }
        workerTask = nullptr;
    }
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

void WeatherPattern::worker()
{
    while (running)
    {
        uint32_t wait = RETRY_MS;
        if (WiFi.status() != WL_CONNECTED)
        {
            setError("No WiFi");
        }
        else
        {
            std::string query;
            bool haveFix, stale;
            uint32_t age;
            {
                Lock l;
                query = shared.location;
                haveFix = shared.located && shared.locatedFor == query;
            }
            if (!haveFix)
            {
                haveFix = locate(query);
            }
            if (haveFix)
            {
                {
                    Lock l;
                    age = millis() - shared.fetchedMs;
                    stale = !shared.current || age >= REFRESH_MS;
                }
                if (stale)
                {
                    wait = fetchForecast() ? REFRESH_MS : RETRY_MS;
                }
                else
                {
                    wait = REFRESH_MS - age;
                }
            }
        }
        // Woken early by a new location, or by end().
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait));
    }
    xSemaphoreGive(workerDone);
    vTaskDelete(NULL);
}

/// Finds latitude and longitude: geocoding `query`, or from the IP address
/// when it is empty.
bool WeatherPattern::locate(const std::string &query)
{
    float lat = 0, lon = 0;
    std::string place;
    bool found = false;
    if (query.empty())
    {
        // Two free services, in case one is down or rate-limiting.
        const char *const URLS[] = {"https://ipapi.co/json/", "https://ipwho.is/"};
        for (const char *url : URLS)
        {
            JsonDocument filter, doc;
            filter["latitude"] = true;
            filter["longitude"] = true;
            filter["city"] = true;
            filter["region"] = true;
            if (getJson(url, filter, doc) && doc["latitude"].is<float>())
            {
                lat = doc["latitude"];
                lon = doc["longitude"];
                place = std::string(doc["city"] | "") + ", " + (doc["region"] | "");
                found = true;
                break;
            }
        }
        if (!found)
        {
            setError("Couldn't find the cube's location from its IP address");
            return false;
        }
    }
    else
    {
        char name[96];
        if (weather::urlEncode(query.c_str(), name, sizeof(name)) < 0)
        {
            setError("Location is too long");
            return false;
        }
        JsonDocument filter, doc;
        filter["results"][0]["latitude"] = true;
        filter["results"][0]["longitude"] = true;
        filter["results"][0]["name"] = true;
        filter["results"][0]["admin1"] = true;
        filter["results"][0]["country_code"] = true;
        const String url = String("https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=") + name;
        if (!getJson(url, filter, doc))
        {
            setError("Location lookup failed");
            return false;
        }
        JsonObject r = doc["results"][0];
        if (r.isNull())
        {
            setError(std::string("Couldn't find \"") + query + "\"");
            return false;
        }
        lat = r["latitude"];
        lon = r["longitude"];
        place = std::string(r["name"] | "") + ", " + (r["admin1"].is<const char *>() ? r["admin1"] : r["country_code"] | "");
    }
    Lock l;
    shared.located = true;
    shared.locatedFor = query;
    shared.lat = lat;
    shared.lon = lon;
    shared.place = place;
    shared.fromIp = query.empty();
    shared.current = false; // that forecast was for somewhere else
    shared.error.clear();
    ESP_LOGI("Weather", "Located %s (%.3f, %.3f)%s", place.c_str(), lat, lon, query.empty() ? " from IP" : "");
    return true;
}

bool WeatherPattern::fetchForecast()
{
    float lat, lon;
    {
        Lock l;
        lat = shared.lat;
        lon = shared.lon;
    }
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code,is_day,wind_speed_10m"
             "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
             lat, lon);
    JsonDocument filter, doc;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"] = true;
    filter["current"]["is_day"] = true;
    filter["current"]["wind_speed_10m"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;
    if (!getJson(url, filter, doc) || !doc["current"]["temperature_2m"].is<float>())
    {
        setError("Couldn't fetch the weather");
        return false;
    }
    Lock l;
    shared.tempC = doc["current"]["temperature_2m"];
    shared.code = doc["current"]["weather_code"] | 3;
    shared.day = (doc["current"]["is_day"] | 1) != 0;
    shared.windKmh = doc["current"]["wind_speed_10m"] | 0.0f;
    shared.highC = doc["daily"]["temperature_2m_max"][0] | shared.tempC;
    shared.lowC = doc["daily"]["temperature_2m_min"][0] | shared.tempC;
    shared.current = true;
    shared.fetchedMs = millis();
    shared.error.clear();
    ESP_LOGI("Weather", "%s: %.1f C, code %d, %s", shared.place.c_str(), shared.tempC, shared.code, shared.day ? "day" : "night");
    return true;
}

/// Sets up clouds and precipitation for a kind of sky.
void WeatherPattern::setScene(weather::Sky sky, float intensity)
{
    scene = sky;
    sceneIntensity = intensity;
    sceneSet = true;
    using weather::Sky;
    float yMin = 4, yMax = 30;
    switch (sky)
    {
    case Sky::CLEAR: cloudCount = 0; break;
    case Sky::PARTLY_CLOUDY: cloudCount = 3; yMin = 8; yMax = 30; break;
    case Sky::FOG: cloudCount = 0; break;
    case Sky::CLOUDY: cloudCount = 8; yMin = 2; yMax = 34; break;
    default: cloudCount = 8; yMin = 0; yMax = 12; break; // low, heavy cloud
    }
    for (int i = 0; i < cloudCount; i++)
    {
        Cloud &c = clouds[i];
        c.x = frand() * (STRIP_W + 40) - 20;
        c.y = yMin + frand() * (yMax - yMin);
        c.scale = 0.8f + frand() * 0.7f;
        c.speed = 1.5f + frand() * 2.5f;
    }
    switch (sky)
    {
    case Sky::DRIZZLE: dropCount = int(25 + 40 * intensity); break;
    case Sky::RAIN: dropCount = int(40 + 110 * intensity); break;
    case Sky::STORM: dropCount = DROPS; break;
    case Sky::SNOW: dropCount = int(40 + 90 * intensity); break;
    default: dropCount = 0; break;
    }
    dropCount = min(dropCount, DROPS);
    for (int i = 0; i < dropCount; i++)
    {
        respawnDrop(drops[i], true);
    }
}

void WeatherPattern::respawnDrop(Drop &d, bool anywhere)
{
    using weather::Sky;
    d.x = frand() * STRIP_W;
    d.y = anywhere ? frand() * STRIP_H : 8 + frand() * 6;
    d.phase = frand() * 6.28f;
    switch (scene)
    {
    case Sky::SNOW:
        d.speed = 7 + frand() * 9;
        d.size = frand() < 0.2f ? 2 : 1;
        break;
    case Sky::DRIZZLE:
        d.speed = 32 + frand() * 18;
        d.size = 1;
        break;
    default:
        d.speed = 75 + frand() * 40;
        d.size = 3;
        break;
    }
}

void WeatherPattern::newBolt()
{
    boltPoints = 0;
    float x = 10 + frand() * (STRIP_W - 20);
    for (int y = 10; y < STRIP_H && boltPoints < 16; y += 4)
    {
        boltX[boltPoints++] = int8_t(constrain(x, 0.0f, float(STRIP_W - 1)));
        x += (frand() - 0.5f) * 8;
    }
}

/// The sky on the side strip, every pixel each frame.
void WeatherPattern::drawSides(const Report &r, float t, float dt)
{
    using weather::Sky;
    const Sky sky = r.current ? weather::sky(r.code) : Sky::CLOUDY;
    const float intensity = r.current ? weather::intensity(r.code) : 0;
    if (!sceneSet || sky != scene || intensity != sceneIntensity)
    {
        setScene(sky, intensity);
    }
    const bool day = r.current ? r.day : false;
    const SkyColors &sc = SKIES[int(sky)];
    const color::RGB skyTop = day ? sc.dayTop : sc.nightTop, skyBottom = day ? sc.dayBottom : sc.nightBottom;
    const color::RGB cloudColor = day ? sc.dayCloud : sc.nightCloud;
    const bool lit = sky == Sky::CLEAR || sky == Sky::PARTLY_CLOUDY;
    const float wind = r.current ? r.windKmh : 0;

    // Lightning: a flash and a bolt every few seconds in a storm.
    const uint32_t now = millis();
    float flash = 0;
    if (sky == Sky::STORM)
    {
        if (int32_t(now - nextStrikeMs) >= 0)
        {
            strikeMs = now;
            nextStrikeMs = now + 2500 + uint32_t(frand() * 5000);
            newBolt();
        }
        const uint32_t since = now - strikeMs;
        if (strikeMs && since < 300)
        {
            // A double flicker.
            flash = since < 80 ? 1.0f : (since > 140 && since < 200 ? 0.6f : 0.0f);
        }
    }

    for (Cloud &c : clouds)
    {
        c.x += (c.speed + wind * 0.06f) * dt;
        if (c.x > STRIP_W + 20)
        {
            c.x = -20;
        }
    }

    // Sun or moon, on the left face.
    const float sunX = 40, sunY = 19;
    const float rays = t * 0.3f;

    for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
    {
        const int sy = STRIP_H - 1 - row;
        const color::RGB base = color::lerp(skyTop, skyBottom, uint8_t(sy * 255 / (STRIP_H - 1)));
        uint8_t *out = pattern->display->rowForWrite(row, cube::FACE_SIZE, STRIP_W);
        // Chain x 64-191 is strip x 127-0.
        for (int sx = STRIP_W - 1; sx >= 0; sx--)
        {
            float cr = base.r, cg = base.g, cb = base.b;
            if (lit)
            {
                const float dx = sx - sunX, dy = sy - sunY;
                const float d2 = dx * dx + dy * dy;
                if (d2 < 22 * 22)
                {
                    const float d = sqrtf(d2);
                    if (day)
                    {
                        if (d < 7.5f)
                        {
                            cr = 255, cg = 225, cb = 110;
                        }
                        else
                        {
                            // Glow, and rays turning slowly round it.
                            float g = 1 - (d - 7.5f) / 14.5f;
                            g = g * g * 0.55f;
                            const float a = atan2f(dy, dx) + rays;
                            const float ray = cosf(a * 6);
                            if (d > 9.5f && d < 15.5f && ray > 0.8f)
                            {
                                g += 0.5f * (ray - 0.8f) / 0.2f;
                            }
                            cr += (255 - cr) * min(g, 1.0f);
                            cg += (215 - cg) * min(g, 1.0f);
                            cb += (90 - cb) * min(g, 1.0f) * 0.5f;
                        }
                    }
                    else if (d < 7)
                    {
                        // A crescent: the disc less one offset from it.
                        const float ox = dx - 3.2f, oy = dy + 2.2f;
                        if (ox * ox + oy * oy > 6.2f * 6.2f)
                        {
                            cr = 225, cg = 225, cb = 205;
                        }
                    }
                    else if (d < 11)
                    {
                        const float g = (11 - d) / 4 * 0.15f;
                        cr += 200 * g, cg += 200 * g, cb += 190 * g;
                    }
                }
            }
            for (int i = 0; i < cloudCount; i++)
            {
                const Cloud &c = clouds[i];
                if (fabsf(sx - c.x) > 16 * c.scale || fabsf(sy - c.y) > 10 * c.scale)
                {
                    continue;
                }
                float cover = 0;
                for (const Puff &p : PUFFS)
                {
                    const float px = sx - (c.x + p.dx * c.scale), py = sy - (c.y + p.dy * c.scale);
                    const float r2 = p.r * p.r * c.scale * c.scale;
                    const float d2 = px * px + py * py;
                    if (d2 < r2)
                    {
                        cover = max(cover, min(1.0f, (r2 - d2) / (r2 * 0.4f)));
                    }
                }
                if (cover > 0)
                {
                    // Darker underneath.
                    const float shade = 1.0f - 0.25f * max(0.0f, (sy - c.y) / (8 * c.scale));
                    cr += (cloudColor.r * shade - cr) * cover;
                    cg += (cloudColor.g * shade - cg) * cover;
                    cb += (cloudColor.b * shade - cb) * cover;
                }
            }
            if (sky == Sky::FOG)
            {
                float f = 0.45f + 0.25f * sinf(sx * 0.08f + t * 0.35f + sy * 0.2f) + 0.15f * sinf(sx * 0.045f - t * 0.22f);
                f *= 0.55f + sy / 140.0f;
                cr += (cloudColor.r - cr) * f;
                cg += (cloudColor.g - cg) * f;
                cb += (cloudColor.b - cb) * f;
            }
            if (flash > 0)
            {
                cr += (200 - cr) * flash * 0.6f;
                cg += (200 - cg) * flash * 0.6f;
                cb += (230 - cb) * flash * 0.6f;
            }
            *out++ = uint8_t(min(cr, 255.0f));
            *out++ = uint8_t(min(cg, 255.0f));
            *out++ = uint8_t(min(cb, 255.0f));
        }
    }

    // Stars on a clear night, twinkling, where nothing covers them.
    if (!day && lit)
    {
        for (int i = 0; i < STARS; i++)
        {
            const int sx = stars[i] % STRIP_W, sy = stars[i] / STRIP_W;
            const float dx = sx - sunX, dy = sy - sunY;
            if (dx * dx + dy * dy < 12 * 12)
            {
                continue;
            }
            const uint8_t v = uint8_t(70 + 80 * (0.5f + 0.5f * sinf(t * (1.5f + (i % 5) * 0.4f) + i)));
            strip->drawPixelRGB888(int16_t(sx), int16_t(sy), v, v, uint8_t(min(255, v + 20)));
        }
    }

    // Rain, drizzle or snow.
    const float slant = -min(wind / 70.0f, 0.6f);
    const bool snow = sky == Sky::SNOW;
    const color::RGB dropColor = snow ? color::RGB{235, 240, 250} : (day ? color::RGB{165, 190, 235} : color::RGB{95, 115, 165});
    for (int i = 0; i < dropCount; i++)
    {
        Drop &d = drops[i];
        d.y += d.speed * dt;
        d.x += (snow ? 4.0f * sinf(t * 1.3f + d.phase) + slant * 8 : slant * d.speed) * dt;
        if (d.x < 0)
        {
            d.x += STRIP_W;
        }
        else if (d.x >= STRIP_W)
        {
            d.x -= STRIP_W;
        }
        if (d.y >= STRIP_H)
        {
            respawnDrop(d, false);
            continue;
        }
        if (snow)
        {
            strip->drawPixelRGB888(int16_t(d.x), int16_t(d.y), dropColor.r, dropColor.g, dropColor.b);
            if (d.size > 1)
            {
                strip->drawPixelRGB888(int16_t(d.x) + 1, int16_t(d.y), dropColor.r, dropColor.g, dropColor.b);
                strip->drawPixelRGB888(int16_t(d.x), int16_t(d.y) + 1, dropColor.r, dropColor.g, dropColor.b);
            }
            continue;
        }
        // A short streak, brightest at the bottom.
        for (int k = 0; k < d.size; k++)
        {
            const float level = float(k + 1) / d.size;
            const int16_t x = int16_t(d.x - slant * k), y = int16_t(d.y - k);
            strip->drawPixelRGB888(x, y, uint8_t(dropColor.r * level), uint8_t(dropColor.g * level), uint8_t(dropColor.b * level));
        }
    }

    // The bolt itself, for a moment after the flash.
    if (sky == Sky::STORM && strikeMs && now - strikeMs < 220)
    {
        for (int i = 0; i + 1 < boltPoints; i++)
        {
            strip->drawLine(boltX[i], int16_t(10 + i * 4), boltX[i + 1], int16_t(14 + i * 4), Canvas::color565(255, 255, 230));
        }
    }
}

/// The temperature, conditions, and high and low on top; or, until there is
/// a forecast, what the pattern is waiting for.
void WeatherPattern::drawTop(const Report &r)
{
    const int temp = weather::temperature(r.tempC, r.metric);
    const int hi = weather::temperature(r.highC, r.metric), lo = weather::temperature(r.lowC, r.metric);
    // Redraw only when something shown changes.
    uint32_t key = uint32_t(r.current) | (uint32_t(r.metric) << 1) | (uint32_t(r.code & 0xFF) << 2) |
                   (uint32_t(temp & 0xFF) << 10) ^ (uint32_t(hi & 0xFF) << 18) ^ (uint32_t(lo & 0xFF) << 24);
    if (!r.current)
    {
        for (const char *c = r.error; *c; c++)
        {
            key = key * 31 + uint8_t(*c);
        }
        key ^= r.located ? 0x55AA : 0xAA55;
    }
    if (key == topKey)
    {
        return;
    }
    topKey = key;

    top->fillScreen(0);
    top->setFont(NULL);
    top->setTextSize(1);
    top->setTextWrap(false);
    auto centered = [this](const char *text, int16_t y, uint16_t c)
    {
        top->setTextColor(c);
        top->setCursor(int16_t((cube::FACE_SIZE - 6 * int(strlen(text))) / 2), y);
        top->print(text);
    };
    if (!r.current)
    {
        centered("WEATHER", 8, Canvas::color565(150, 150, 160));
        const char *status = r.error[0] ? r.error : (r.located ? "Fetching" : "Locating");
        // Up to three lines of ten characters.
        char line[11];
        const size_t len = strlen(status);
        for (int i = 0; i < 3 && size_t(i * 10) < len; i++)
        {
            strlcpy(line, status + i * 10, sizeof(line));
            centered(line, int16_t(26 + i * 10), Canvas::color565(r.error[0] ? 255 : 120, r.error[0] ? 120 : 160, r.error[0] ? 90 : 220));
        }
        return;
    }

    centered(weather::describe(r.code), 5, Canvas::color565(170, 170, 185));

    // The temperature, coloured from cold blue to hot orange.
    static const color::RGB STOPS[] = {{120, 170, 255}, {175, 220, 255}, {255, 245, 225}, {255, 200, 90}, {255, 110, 50}};
    const float k = constrain((r.tempC + 5) / 40.0f, 0.0f, 1.0f);
    const color::RGB tc = color::gradient(STOPS, 5, uint8_t(k * 255));
    char text[8];
    snprintf(text, sizeof(text), "%d", temp);
    int16_t x1, y1;
    uint16_t w, h;
    top->setFont(&FreeSansBold18pt7b);
    top->getTextBounds(text, 0, 40, &x1, &y1, &w, &h);
    // Number, degree ring and unit letter, centred together.
    const int16_t total = int16_t(w) + 11;
    const int16_t x = int16_t((cube::FACE_SIZE - total) / 2 - x1);
    top->setTextColor(Canvas::color565(tc.r, tc.g, tc.b));
    top->setCursor(x, 40);
    top->print(text);
    top->setFont(NULL);
    const int16_t right = int16_t(x + x1 + int16_t(w));
    top->drawCircle(int16_t(right + 3), int16_t(y1 + 2), 2, Canvas::color565(tc.r, tc.g, tc.b));
    top->setTextColor(Canvas::color565(tc.r, tc.g, tc.b));
    top->setCursor(int16_t(right + 6), int16_t(y1 + 6));
    top->print(r.metric ? "C" : "F");

    snprintf(text, sizeof(text), "H%d", hi);
    top->setTextColor(Canvas::color565(255, 170, 120));
    top->setCursor(8, 51);
    top->print(text);
    snprintf(text, sizeof(text), "L%d", lo);
    top->setTextColor(Canvas::color565(130, 180, 255));
    top->setCursor(int16_t(56 - 6 * int(strlen(text))), 51);
    top->print(text);
}

void WeatherPattern::tick()
{
    if (strip == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;
    const Report r = report();
    drawSides(r, (now - startMs) / 1000.0f, dt);
    drawTop(r);
}
