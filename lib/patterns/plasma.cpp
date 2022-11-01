#include "plasma.h"

void plasma()
{
  frameCount++;
  uint16_t t = fastCosineCalc((42 * frameCount) >> 6); // time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount) >> 6);
  uint16_t t3 = fastCosineCalc((38 * frameCount) >> 6);

  for (uint8_t i = 0; i < PANEL_WIDTH * PANELS_NUMBER; i++)
  {
    for (uint8_t j = 0; j < PANEL_HEIGHT; j++)
    {
      uint8_t x = (projCalcIntX(i, j) + 66);
      uint8_t y = (projCalcY(i, j) + 66);

      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3)))) >> 2);
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3)))) >> 2);
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2)))) >> 2);

      dma_display->drawPixelRGB888(i, j, r, g, b);
    }
  }
}