#include "plasma.h"

Plasma::Plasma()
{
  data.name = "Plasma";
}

Plasma::~Plasma()
{
  stop();
}

void Plasma::init(PatternServices *pattern)
{
  this->pattern = pattern;
  frameCount = 25500;
}

void Plasma::start()
{
  xTaskCreate(
      [](void *o)
      { static_cast<Plasma *>(o)->show(); }, // This is disgusting, but it works
      "Plasma - Refresh",                    // Name of the task (for debugging)
      4000,                                  // Stack size (bytes)
      this,                                  // Parameter to pass
      1,                                     // Task priority
      &refreshTask                           // Task handle
  );
}

void Plasma::stop() {
  vTaskDelete(refreshTask);
}

void Plasma::show()
{
  frameCount++;
  uint16_t t = fast_cos((42 * frameCount) >> 6); // time displacement - fiddle with these til it looks good...
  uint16_t t2 = fast_cos((35 * frameCount) >> 6);
  uint16_t t3 = fast_cos((38 * frameCount) >> 6);

  for (uint8_t i = 0; i < PANEL_WIDTH * PANELS_NUMBER; i++)
  {
    for (uint8_t j = 0; j < PANEL_HEIGHT; j++)
    {
      uint8_t x = (PROJ_CALC_INT_X(i, j) + 66);
      uint8_t y = (PROJ_CALC_Y(i, j) + 66);

      uint8_t r = fast_cos(((x << 3) + (t >> 1) + fast_cos((t2 + (y << 3)))) >> 2);
      uint8_t g = fast_cos(((y << 3) + t + fast_cos(((t3 >> 2) + (x << 3)))) >> 2);
      uint8_t b = fast_cos(((y << 3) + t2 + fast_cos((t + x + (g >> 2)))) >> 2);

      pattern->display->drawPixelRGB888(i, j, r, g, b);
    }
  }
}