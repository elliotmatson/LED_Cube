#include "plasma.h"

unsigned long frameCount = 25500;

void plasma()
{
  frameCount++;
  uint16_t t = FAST_COS((42 * frameCount) >> 6); // time displacement - fiddle with these til it looks good...
  uint16_t t2 = FAST_COS((35 * frameCount) >> 6);
  uint16_t t3 = FAST_COS((38 * frameCount) >> 6);

  for (uint8_t i = 0; i < PANEL_WIDTH * PANELS_NUMBER; i++)
  {
    for (uint8_t j = 0; j < PANEL_HEIGHT; j++)
    {
      uint8_t x = (PROJ_CALC_INT_X(i, j) + 66);
      uint8_t y = (PROJ_CALC_Y(i, j) + 66);

      uint8_t r = FAST_COS(((x << 3) + (t >> 1) + FAST_COS((t2 + (y << 3)))) >> 2);
      uint8_t g = FAST_COS(((y << 3) + t + FAST_COS(((t3 >> 2) + (x << 3)))) >> 2);
      uint8_t b = FAST_COS(((y << 3) + t2 + FAST_COS((t + x + (g >> 2)))) >> 2);

      dma_display->drawPixelRGB888(i, j, r, g, b);
    }
  }
}