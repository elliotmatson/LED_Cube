#include "clock.h"

#include "fonts.h"
#include <math.h>

static const char *WEEKDAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

Clock::Clock()
{
  data.id = "clock";
  data.name = "Clock";
}

Clock::~Clock()
{
  end();
}

void Clock::begin(PatternServices *services)
{
  pattern = services;
  // Rotation 2 turns the side faces upright, as Spotify does.
  timeFace = new SinglePanel(*pattern->display, 1, 2);
  dateFace = new SinglePanel(*pattern->display, 2, 2);
  lastSecond = lastMinute = lastDay = -1;
  waitingShown = false;
}

void Clock::end()
{
  delete timeFace;
  delete dateFace;
  timeFace = dateFace = nullptr;
}

void Clock::tick()
{
  struct tm t;
  // No wait: getLocalTime() otherwise spins for up to 5 s while the time is
  // unset, which would stall the render task.
  if (!getLocalTime(&t, 0))
  {
    drawWaiting();
    return;
  }
  if (waitingShown)
  {
    pattern->display->fillScreen(0);
    waitingShown = false;
  }
  if (t.tm_sec != lastSecond)
  {
    drawDial(t);
    lastSecond = t.tm_sec;
  }
  if (t.tm_min != lastMinute)
  {
    drawTime(t);
    lastMinute = t.tm_min;
  }
  if (t.tm_mday != lastDay)
  {
    drawDate(t);
    lastDay = t.tm_mday;
  }
}

void Clock::drawWaiting()
{
  if (waitingShown)
  {
    return;
  }
  pattern->display->fillScreen(0);
  timeFace->setFont(NULL);
  timeFace->setTextSize(1);
  timeFace->setTextColor(0xFFFF);
  timeFace->setCursor(4, 24);
  timeFace->print("Waiting\n    for time");
  waitingShown = true;
  lastSecond = lastMinute = lastDay = -1;
}

void Clock::centered(SinglePanel *face, const char *text, int16_t baseline)
{
  int16_t x1, y1;
  uint16_t w, h;
  face->getTextBounds(text, 0, baseline, &x1, &y1, &w, &h);
  face->setCursor((cube::FACE_SIZE - int16_t(w)) / 2 - x1, baseline);
  face->print(text);
}

void Clock::drawTime(const struct tm &t)
{
  char hours[3], minutes[3];
  int h12 = t.tm_hour % 12;
  snprintf(hours, sizeof(hours), "%d", h12 == 0 ? 12 : h12);
  snprintf(minutes, sizeof(minutes), "%02d", t.tm_min);
  timeFace->fillScreen(0);
  timeFace->setFont(&FreeSansBold18pt7b);
  timeFace->setTextSize(1);
  timeFace->setTextColor(0xFFFF);
  centered(timeFace, hours, 28);
  timeFace->setTextColor(Canvas::color565(150, 190, 255));
  centered(timeFace, minutes, 60);
}

void Clock::drawDate(const struct tm &t)
{
  char day[3];
  snprintf(day, sizeof(day), "%d", t.tm_mday);
  dateFace->fillScreen(0);
  dateFace->setTextSize(1);
  dateFace->setFont(&FreeSansBold12pt7b);
  dateFace->setTextColor(Canvas::color565(255, 140, 0));
  centered(dateFace, WEEKDAYS[t.tm_wday % 7], 19);
  dateFace->setFont(&FreeSansBold18pt7b);
  dateFace->setTextColor(0xFFFF);
  centered(dateFace, day, 46);
  dateFace->setFont(&FreeSansBold9pt7b);
  dateFace->setTextColor(Canvas::color565(160, 160, 160));
  centered(dateFace, MONTHS[t.tm_mon % 12], 62);
}

void Clock::drawDial(const struct tm &t)
{
  Canvas *c = pattern->display;
  // The top face is face 0, unrotated: chain (0..63, 0..63). Seen from the
  // shared corner (63, 63), "up" is towards (0, 0) and "right" towards
  // (63, 0), the edge shared with the right face.
  const float cx = 31.5f, cy = 31.5f;
  const float fx = -0.70710678f, fy = -0.70710678f; // 12 o'clock
  const float rx = 0.70710678f, ry = -0.70710678f;  // 3 o'clock
  auto point = [&](float angle, float radius, int16_t &x, int16_t &y)
  {
    float s = sinf(angle), co = cosf(angle);
    x = int16_t(lroundf(cx + radius * (co * fx + s * rx)));
    y = int16_t(lroundf(cy + radius * (co * fy + s * ry)));
  };
  auto hand = [&](float angle, float length, uint16_t color)
  {
    int16_t x, y;
    point(angle, length, x, y);
    c->drawLine(31, 31, x, y, color);
    c->drawLine(32, 32, x, y, color);
  };

  c->fillRect(0, 0, cube::FACE_SIZE, cube::FACE_SIZE, 0);
  for (int i = 0; i < 12; i++)
  {
    int16_t x0, y0, x1, y1;
    float a = i * float(M_PI) / 6;
    point(a, i % 3 == 0 ? 26 : 28, x0, y0);
    point(a, 30, x1, y1);
    c->drawLine(x0, y0, x1, y1, i % 3 == 0 ? Canvas::color565(200, 200, 200) : Canvas::color565(70, 70, 70));
  }
  const float minute = t.tm_min + t.tm_sec / 60.0f;
  const float hour = (t.tm_hour % 12) + minute / 60.0f;
  hand(hour * float(M_PI) / 6, 15, 0xFFFF);
  hand(minute * float(M_PI) / 30, 23, Canvas::color565(150, 190, 255));
  int16_t sx, sy;
  point(t.tm_sec * float(M_PI) / 30, 26, sx, sy);
  c->drawLine(31, 31, sx, sy, Canvas::color565(255, 40, 40));
  c->fillRect(30, 30, 4, 4, Canvas::color565(255, 40, 40));
}
