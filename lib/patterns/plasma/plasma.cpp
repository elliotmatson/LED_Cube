#include "plasma.h"

Plasma::Plasma()
{
  data.id = "plasma";
  data.name = "Plasma";
}

Plasma::~Plasma()
{
  end();
}

void Plasma::begin(PatternServices *services)
{
  pattern = services;
  startMs = millis();
  projected = static_cast<uint8_t *>(heap_caps_malloc(cube::CELLS * 2, MALLOC_CAP_SPIRAM));
  if (projected == nullptr)
  {
    return;
  }
  for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
  {
    for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++)
    {
      uint8_t *p = projected + (y * cube::CHAIN_WIDTH + x) * 2;
      p[0] = uint8_t(cube::projectX(x, y) + 66);
      p[1] = uint8_t(cube::projectY(x, y) + 66);
    }
  }
}

void Plasma::end()
{
  free(projected);
  projected = nullptr;
}

void Plasma::tick()
{
  if (projected == nullptr)
  {
    return;
  }
  // Animation follows the clock, not the frame count, so a faster tick makes
  // it smoother rather than faster. It advances as it did when it ran at 26
  // frames a second, from the same starting point (frame 25500). `phase` is
  // that frame count in 1/64ths, so (42 * frame) >> 6 becomes
  // (42 * phase) >> 12.
  const uint64_t PHASE_PER_SECOND = 26 * 64;
  const uint64_t phase = 25500ULL * 64 + uint64_t(millis() - startMs) * PHASE_PER_SECOND / 1000;
  uint16_t t = fast_cos((42 * phase) >> 12); // time displacement - fiddle with these til it looks good...
  uint16_t t2 = fast_cos((35 * phase) >> 12);
  uint16_t t3 = fast_cos((38 * phase) >> 12);

  const uint8_t *proj = projected;
  for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
  {
    uint8_t *out = pattern->display->rowForWrite(row, 0, cube::CHAIN_WIDTH);
    for (int16_t col = 0; col < cube::CHAIN_WIDTH; col++, proj += 2)
    {
      uint8_t x = proj[0];
      uint8_t y = proj[1];

      uint8_t r = fast_cos(((x << 3) + (t >> 1) + fast_cos((t2 + (y << 3)))) >> 2);
      uint8_t g = fast_cos(((y << 3) + t + fast_cos(((t3 >> 2) + (x << 3)))) >> 2);
      uint8_t b = fast_cos(((y << 3) + t2 + fast_cos((t + x + (g >> 2)))) >> 2);

      *out++ = r;
      *out++ = g;
      *out++ = b;
    }
  }
}
