#include "ticker.h"

#include <Fonts/FreeSansBold18pt7b.h>
#include <math.h>

namespace
{
    // Created on first use rather than during static initialization.
    SemaphoreHandle_t messageLock()
    {
        static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        return lock;
    }
    std::string customMessage;

    const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char *MONTHS[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
}

void Ticker::setMessage(const std::string &text)
{
    xSemaphoreTake(messageLock(), portMAX_DELAY);
    customMessage = text;
    xSemaphoreGive(messageLock());
}

std::string Ticker::message()
{
    xSemaphoreTake(messageLock(), portMAX_DELAY);
    std::string copy = customMessage;
    xSemaphoreGive(messageLock());
    return copy;
}

Ticker::Ticker()
{
    data.id = "ticker";
    data.name = "Ticker";
}

Ticker::~Ticker()
{
    end();
}

void Ticker::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    strip->setFont(&FreeSansBold18pt7b);
    strip->setTextSize(1);
    strip->setTextWrap(false);
    topHue = static_cast<uint8_t *>(heap_caps_malloc(cube::FACE_SIZE * cube::FACE_SIZE, MALLOC_CAP_SPIRAM));
    if (topHue)
    {
        for (int y = 0; y < cube::FACE_SIZE; y++)
        {
            for (int x = 0; x < cube::FACE_SIZE; x++)
            {
                const float a = atan2f(y - 31.5f, x - 31.5f);
                topHue[y * cube::FACE_SIZE + x] = uint8_t((a + float(M_PI)) * (256 / (2 * float(M_PI))));
            }
        }
    }
    which = 0;
    nextMessage();
}

void Ticker::end()
{
    delete strip;
    strip = nullptr;
    free(topHue);
    topHue = nullptr;
}

void Ticker::nextMessage()
{
    // Custom message (if any), time, date, round again.
    for (int tries = 0; tries < 3; tries++)
    {
        const int current = which;
        which = (which + 1) % 3;
        struct tm t;
        const bool haveTime = getLocalTime(&t, 0);
        char buf[64];
        if (current == 0)
        {
            text = message();
        }
        else if (current == 1 && haveTime)
        {
            const int h12 = t.tm_hour % 12;
            snprintf(buf, sizeof(buf), "%d:%02d %s", h12 == 0 ? 12 : h12, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
            text = buf;
        }
        else if (current == 2 && haveTime)
        {
            snprintf(buf, sizeof(buf), "%s, %s %d", WEEKDAYS[t.tm_wday % 7], MONTHS[t.tm_mon % 12], t.tm_mday);
            text = buf;
        }
        else
        {
            text.clear();
        }
        if (!text.empty())
        {
            break;
        }
    }
    if (text.empty())
    {
        text = "LED Cube";
    }
    int16_t x1, y1;
    uint16_t w, h;
    strip->getTextBounds(text.c_str(), 0, 44, &x1, &y1, &w, &h);
    textWidth = int16_t(w) + x1;
    offset = strip->width();
    hue += 40;
}

void Ticker::tick()
{
    if (!strip)
    {
        return;
    }
    if (--offset < -textWidth - 16)
    {
        nextMessage();
    }
    strip->fillScreen(0);
    color::RGB c = color::hsv(hue, 180, 255);
    strip->setTextColor(Canvas::color565(c.r, c.g, c.b));
    strip->setCursor(offset, 44);
    strip->print(text.c_str());

    if (topHue)
    {
        const uint8_t spin = uint8_t(millis() / 40);
        const uint8_t *h = topHue;
        for (int16_t y = 0; y < cube::FACE_SIZE; y++)
        {
            uint8_t *out = pattern->display->rowForWrite(y, 0, cube::FACE_SIZE);
            for (int16_t x = 0; x < cube::FACE_SIZE; x++, h++)
            {
                color::RGB t = color::hsv(uint8_t(*h + spin), 255, 50);
                *out++ = t.r;
                *out++ = t.g;
                *out++ = t.b;
            }
        }
    }
}
