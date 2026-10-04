#include "plasma.h"
#include "color.h"

namespace
{
  // How far in from an outer edge the fade reaches, in pixels.
  const float FADE_WIDTH = 26.0f;

  /// 0 at an outer edge (coordinate 0), rising smoothly to 1 at FADE_WIDTH.
  float edgeFade(float d)
  {
    const float t = d >= FADE_WIDTH ? 1.0f : (d <= 0 ? 0.0f : d / FADE_WIDTH);
    return t * t * (3 - 2 * t);
  }
}

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
  fade = static_cast<uint8_t *>(heap_caps_malloc(cube::CELLS, MALLOC_CAP_SPIRAM));
  if (projected == nullptr || fade == nullptr)
  {
    end();
    return;
  }
  for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
  {
    for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++)
    {
      uint8_t *p = projected + (y * cube::CHAIN_WIDTH + x) * 2;
      p[0] = uint8_t(cube::projectX(x, y) + 66);
      p[1] = uint8_t(cube::projectY(x, y) + 66);
      // The outer edges are where a coordinate is 0; the face's own axis is
      // 64, so the product fades only towards the two outer edges it has.
      const cube::Vec3 v = cube::toCube({x, y});
      fade[y * cube::CHAIN_WIDTH + x] = uint8_t(255 * edgeFade(v.x) * edgeFade(v.y) * edgeFade(v.z));
    }
  }
}

void Plasma::end()
{
  free(projected);
  free(fade);
  projected = nullptr;
  fade = nullptr;
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
  // A second, broader layer: the plasma as a hue at full saturation,
  // flowing round the colour wheel. Time in 1/256ths of a cycle.
  const uint32_t ms = millis() - startMs;
  const uint8_t ta = uint8_t(ms / 40);
  const uint8_t tb = uint8_t(ms / 55);
  const uint8_t tc = uint8_t(ms / 70);
  const uint8_t drift = uint8_t(ms / 120); // the whole wheel, every 30 s

  const uint8_t *proj = projected;
  const uint8_t *level = fade;
  for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
  {
    uint8_t *out = pattern->display->rowForWrite(row, 0, cube::CHAIN_WIDTH);
    for (int16_t col = 0; col < cube::CHAIN_WIDTH; col++, proj += 2)
    {
      uint8_t x = proj[0];
      uint8_t y = proj[1];

      // Half the spatial frequency of the original: broad colour regions.
      uint8_t r = fast_cos(((x << 2) + (t >> 1) + fast_cos((t2 + (y << 2)))) >> 2);
      uint8_t g = fast_cos(((y << 2) + t + fast_cos(((t3 >> 2) + (x << 2)))) >> 2);
      uint8_t b = fast_cos(((y << 2) + t2 + fast_cos((t + (x >> 1) + (g >> 2)))) >> 2);
      // The hue layer: three waves about half a cycle across the cube, their
      // sum spanning most of the wheel. Averaged with the classic plasma, it
      // gives broad, saturated gradients with the plasma's softer variation.
      const uint16_t v = fast_cos(x + ta) + fast_cos(y - tb) + fast_cos(((x + y) >> 1) + tc);
      const color::RGB c = color::hsv(uint8_t(v / 2 + drift), 255, 255);
      r = uint8_t((r + c.r) >> 1);
      g = uint8_t((g + c.g) >> 1);
      b = uint8_t((b + c.b) >> 1);
      const uint16_t l = *level++;
      *out++ = uint8_t(r * l >> 8);
      *out++ = uint8_t(g * l >> 8);
      *out++ = uint8_t(b * l >> 8);
    }
  }
}
