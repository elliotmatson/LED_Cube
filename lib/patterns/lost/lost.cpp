#include "lost.h"

#include "fonts.h"

namespace
{
    SemaphoreHandle_t messageLock()
    {
        static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        return lock;
    }
    std::string message = "This cube is lost. Please contact its owner.";
    std::string label;
}

void LostPattern::setMessage(const std::string &text)
{
    xSemaphoreTake(messageLock(), portMAX_DELAY);
    message = text.empty() ? "This cube is lost. Please contact its owner." : text;
    xSemaphoreGive(messageLock());
}

void LostPattern::setLabel(const std::string &l)
{
    xSemaphoreTake(messageLock(), portMAX_DELAY);
    label = l;
    xSemaphoreGive(messageLock());
}

LostPattern::LostPattern()
{
    data.id = "lost";
    data.name = "Lost";
    data.hidden = true;
}

LostPattern::~LostPattern()
{
    end();
}

void LostPattern::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    strip->setFont(&FreeSansBold18pt7b);
    strip->setTextSize(1);
    strip->setTextWrap(false);
    offset = strip->width();
    textWidth = 0;
    shownBlink = -1;
}

void LostPattern::end()
{
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

/// LOST in red, on or dim, over the cube's short ID.
void LostPattern::drawTop(bool on)
{
    top->fillScreen(0);
    top->setFont(&FreeSansBold12pt7b);
    top->setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    top->getTextBounds("LOST", 0, 34, &x1, &y1, &w, &h);
    top->setTextColor(on ? Canvas::color565(255, 30, 20) : Canvas::color565(70, 8, 5));
    top->setCursor((cube::FACE_SIZE - int16_t(w)) / 2 - x1, 34);
    top->print("LOST");
    top->setFont(NULL);
    xSemaphoreTake(messageLock(), portMAX_DELAY);
    const std::string id = label.substr(0, 8);
    xSemaphoreGive(messageLock());
    top->setTextColor(Canvas::color565(150, 150, 150));
    top->setCursor(int16_t((cube::FACE_SIZE - 6 * int(id.size())) / 2), 46);
    top->print(id.c_str());
}

void LostPattern::tick()
{
    if (strip == nullptr)
    {
        return;
    }
    const int blink = (millis() / 600) & 1;
    if (blink != shownBlink)
    {
        drawTop(blink != 0);
        shownBlink = blink;
    }

    // Scroll the message; pick up a new one each time it has gone by.
    if (offset < -textWidth)
    {
        xSemaphoreTake(messageLock(), portMAX_DELAY);
        text = message;
        xSemaphoreGive(messageLock());
        int16_t x1, y1;
        uint16_t w, h;
        strip->getTextBounds(text.c_str(), 0, 44, &x1, &y1, &w, &h);
        textWidth = int16_t(w) + x1;
        offset = strip->width();
    }
    if (text.empty())
    {
        xSemaphoreTake(messageLock(), portMAX_DELAY);
        text = message;
        xSemaphoreGive(messageLock());
        int16_t x1, y1;
        uint16_t w, h;
        strip->getTextBounds(text.c_str(), 0, 44, &x1, &y1, &w, &h);
        textWidth = int16_t(w) + x1;
    }
    strip->fillRect(0, 0, strip->width(), strip->height(), 0);
    strip->setTextColor(Canvas::color565(255, 200, 60));
    strip->setCursor(offset, 44);
    strip->print(text.c_str());
    offset--;
}
