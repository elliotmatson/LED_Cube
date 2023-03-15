#include "clock.h"

Clock::Clock()
{
  data.name = "Clock";
}

Clock::~Clock()
{
  stop();
}

void Clock::init(PatternServices *pattern)
{
  this->pattern = pattern;
}

void Clock::start()
{
  xTaskCreate(
      [](void *o)
      { static_cast<Clock *>(o)->show(); }, // This is disgusting, but it works
      "Clock - Refresh",                    // Name of the task (for debugging)
      8000,                                 // Stack size (bytes)
      this,                                 // Parameter to pass
      1,                                    // Task priority
      &refreshTask                          // Task handle
  );
}

void Clock::stop()
{
  vTaskDelete(refreshTask);
}

void Clock::show()
{
  while (true)
  {
    if (!getLocalTime(&timeinfo))
    {
      ESP_LOGE(__func__, "Failed to obtain time");
    }
    pattern->display->fillScreen(0);
    pattern->display->setTextColor(0xFFFF);
    pattern->display->setTextSize(1);
    pattern->display->setCursor(0, 0);
    pattern->display->printf("%.2d:%.2d:%.2d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}