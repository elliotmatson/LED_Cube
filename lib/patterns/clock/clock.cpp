#include "clock.h"

Clock::Clock(PatternServices *pattern)
{
  this->pattern = pattern;
}

Clock::~Clock() {}

void Clock::init() {}

void Clock::show()
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
}