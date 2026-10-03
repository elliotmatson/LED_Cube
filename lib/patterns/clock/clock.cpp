#include "clock.h"

Clock::Clock()
{
  data.id = "clock";
  data.name = "Clock";
}

void Clock::begin(PatternServices *services)
{
  pattern = services;
}

void Clock::tick()
{
  // No wait: getLocalTime() otherwise spins for up to 5 s while the time is
  // unset, which would stall the render task.
  if (!getLocalTime(&timeinfo, 0))
  {
    return;
  }
  // Cleared and redrawn every second, but on the canvas: the panels only see
  // the finished frame, so this no longer flickers.
  pattern->display->fillScreen(0);
  pattern->display->setTextColor(0xFFFF);
  pattern->display->setTextSize(1);
  pattern->display->setCursor(0, 0);
  pattern->display->printf("%.2d:%.2d:%.2d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}
