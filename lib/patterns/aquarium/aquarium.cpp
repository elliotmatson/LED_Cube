#include "aquarium.h"

#include <math.h>
#include "particles.h"

namespace
{
    // One cycle, in PSRAM while the pattern runs: as a global it took 1 KB
    // of scarce internal RAM all the time.
    float *SIN = nullptr;
    inline float sinAt(float x) { return SIN[uint8_t(int32_t(x))]; }

    // Fish, facing left: B body, S stripe, F fin/tail, K eye, . clear.
    struct Sprite
    {
        const char *const *rows;
        int16_t w, h;
        color::RGB body, stripe, fin;
    };
    const char *const SMALL[] = {
        "..BBBB....",
        ".BBSBBB.F.",
        "BKBSBBBBFF",
        ".BBSBBB.F.",
        "..BBBB....",
    };
    const char *const TALL[] = {
        "....BB.....",
        "..BBBBB....",
        ".BBBBBBB.FF",
        "BKBBSBBBBFF",
        ".BBBBBBB.FF",
        "..BBBBB....",
        "....BB.....",
    };
    const char *const LONG[] = {
        "...BBBBBB.....",
        ".BBBBBBBBBB.FF",
        "BKBBBBBBBBBBFF",
        ".BSSSSSSSBB.FF",
        "...BBBBBB.....",
    };
    const Sprite SPRITES[] = {
        {SMALL, 10, 5, {255, 110, 0}, {255, 255, 255}, {255, 140, 40}}, // clownfish
        {TALL, 11, 7, {255, 220, 0}, {255, 240, 120}, {255, 200, 0}},   // yellow tang
        {TALL, 11, 7, {30, 80, 255}, {10, 20, 60}, {255, 210, 0}},      // blue tang
        {SMALL, 10, 5, {220, 30, 60}, {255, 160, 160}, {200, 20, 40}},  // red
        {LONG, 14, 5, {170, 180, 200}, {80, 140, 255}, {150, 160, 180}}, // silver, blue stripe
        {SMALL, 10, 5, {180, 60, 255}, {255, 230, 0}, {255, 230, 0}},   // royal gramma
    };
    const int SPRITE_COUNT = sizeof(SPRITES) / sizeof(SPRITES[0]);

    const int SAND_ROWS = 5;
    const float BUBBLES_PER_S = 5.0f;
    const cube::Vec3 RISE = {0, 0, 0};
    const float RIPPLE_S = 0.8f;
}

Aquarium::Aquarium()
{
    data.id = "aquarium";
    data.name = "Aquarium";
}

Aquarium::~Aquarium()
{
    end();
}

uint32_t Aquarium::random()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

float Aquarium::frand()
{
    return (random() & 0xFFFF) / 65535.0f;
}

void Aquarium::begin(PatternServices *services)
{
    pattern = services;
    rng = esp_random() | 1;
    SIN = static_cast<float *>(heap_caps_malloc(256 * sizeof(float), MALLOC_CAP_SPIRAM));
    if (SIN == nullptr)
    {
        return;
    }
    for (int i = 0; i < 256; i++)
    {
        SIN[i] = sinf(i * 2 * float(M_PI) / 256);
    }
    background = static_cast<uint8_t *>(heap_caps_malloc(STRIP_W * STRIP_H * 3, MALLOC_CAP_SPIRAM));
    if (background == nullptr)
    {
        return;
    }
    // Water darkening with depth, then sand with a little texture.
    for (int sy = 0; sy < STRIP_H; sy++)
    {
        for (int sx = 0; sx < STRIP_W; sx++)
        {
            uint8_t *px = background + (sy * STRIP_W + sx) * 3;
            color::RGB c;
            if (sy >= STRIP_H - SAND_ROWS)
            {
                const int grain = int(random() % 40);
                c = {uint8_t(150 + grain), uint8_t(120 + grain), uint8_t(60 + grain / 2)};
                c = color::scale(c, 150);
            }
            else
            {
                c = color::lerp({0, 70, 130}, {0, 12, 45}, uint8_t(sy * 255 / STRIP_H));
            }
            px[0] = c.r;
            px[1] = c.g;
            px[2] = c.b;
        }
    }
    for (int i = 0; i < WEEDS; i++)
    {
        weeds[i].x = int16_t(8 + i * (STRIP_W - 16) / (WEEDS - 1) + int(random() % 7) - 3);
        weeds[i].height = int16_t(16 + random() % 26);
        weeds[i].phase = frand() * 256;
        weeds[i].color = color::lerp({20, 140, 40}, {80, 200, 60}, uint8_t(random()));
    }
    for (int i = 0; i < FISH; i++)
    {
        Fish &f = fish[i];
        f.kind = i % SPRITE_COUNT;
        f.x = frand() * STRIP_W;
        f.y = 6 + frand() * (STRIP_H - SAND_ROWS - 16);
        f.speed = 6 + frand() * 9;
        f.phase = frand() * 256;
        f.dir = (random() & 1) ? 1 : -1;
    }
    for (Bubble &b : bubbles)
    {
        b.alive = false;
    }
    for (Ripple &r : ripples)
    {
        r.alive = false;
    }
    startMs = lastMs = millis();
    bubbleDebt = 0;
}

void Aquarium::end()
{
    free(background);
    background = nullptr;
    free(SIN);
    SIN = nullptr;
}

/// Sets strip pixel (sx, sy), y down from the top.
void Aquarium::drawStrip(int sx, int sy, color::RGB c)
{
    if (sx < 0 || sx >= STRIP_W || sy < 0 || sy >= STRIP_H)
    {
        return;
    }
    pattern->display->drawPixelRGB888(int16_t(cube::CHAIN_WIDTH - 1 - sx), int16_t(STRIP_H - 1 - sy), c.r, c.g, c.b);
}

void Aquarium::drawFish(const Fish &f)
{
    const Sprite &s = SPRITES[f.kind];
    const float t = (millis() - startMs) / 1000.0f;
    const int x0 = int(f.x) - s.w / 2;
    const int y0 = int(f.y + 1.5f * sinAt(t * 20.0f + f.phase)) - s.h / 2;
    for (int r = 0; r < s.h; r++)
    {
        for (int c = 0; c < s.w; c++)
        {
            // The sprites face left; mirror them to swim right.
            const char k = s.rows[r][f.dir < 0 ? c : s.w - 1 - c];
            color::RGB col;
            switch (k)
            {
            case 'B': col = s.body; break;
            case 'S': col = s.stripe; break;
            case 'F': col = s.fin; break;
            case 'K': col = {10, 10, 10}; break;
            default: continue;
            }
            drawStrip(x0 + c, y0 + r, col);
        }
    }
}

/// The surface from above: blue water with moving caustics, and ripples.
void Aquarium::drawTop(float t)
{
    for (int16_t y = 0; y < cube::FACE_SIZE; y++)
    {
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::FACE_SIZE);
        for (int16_t x = 0; x < cube::FACE_SIZE; x++)
        {
            // Bright where three slow waves nearly cancel: a web of light.
            const float v = sinAt(x * 11.0f + t * 23.0f) + sinAt(y * 13.0f - t * 17.0f) +
                            sinAt((x + y) * 7.0f + t * 11.0f) + sinAt((x - y) * 9.0f - t * 14.0f);
            float k = 1.0f - fabsf(v) * 0.7f;
            k = k < 0 ? 0 : k * k * k;
            *out++ = uint8_t(10 + 110 * k);
            *out++ = uint8_t(70 + 150 * k);
            *out++ = uint8_t(130 + 120 * k);
        }
    }
    for (Ripple &r : ripples)
    {
        if (!r.alive)
        {
            continue;
        }
        const float k = r.age / RIPPLE_S;
        if (k >= 1)
        {
            r.alive = false;
            continue;
        }
        const float radius = 1 + 6 * k;
        const uint8_t level = uint8_t(255 * (1 - k));
        for (int a = 0; a < 256; a += 8)
        {
            const int16_t px = int16_t(r.x + radius * SIN[uint8_t(a + 64)]), py = int16_t(r.y + radius * SIN[uint8_t(a)]);
            if (px >= 0 && px < cube::FACE_SIZE && py >= 0 && py < cube::FACE_SIZE)
            {
                uint8_t *p = pattern->display->rowForWrite(py, px, 1);
                p[0] = max(p[0], uint8_t(level * 7 / 10));
                p[1] = max(p[1], uint8_t(level * 9 / 10));
                p[2] = max(p[2], level);
            }
        }
    }
}

void Aquarium::tick()
{
    if (background == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float t = (now - startMs) / 1000.0f;
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;
    Canvas &canvas = *pattern->display;

    // The tank: water and sand, with slanting light rays drifting across.
    for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
    {
        const int sy = STRIP_H - 1 - row;
        uint8_t *out = canvas.rowForWrite(row, cube::FACE_SIZE, STRIP_W);
        const uint8_t *bg = background + sy * STRIP_W * 3;
        const bool water = sy < STRIP_H - SAND_ROWS;
        // Chain x 64-191 is strip x 127-0.
        for (int sx = STRIP_W - 1; sx >= 0; sx--)
        {
            const uint8_t *b = bg + sx * 3;
            int boost = 0;
            if (water)
            {
                const float ray = sinAt(sx * 4.0f + sy * 2.0f - t * 9.0f) + sinAt(sx * 2.5f + sy * 1.2f + t * 5.0f);
                boost = ray > 1.2f ? int((ray - 1.2f) * 40 * (1.0f - sy / float(STRIP_H))) : 0;
            }
            *out++ = uint8_t(min(255, b[0] + boost / 2));
            *out++ = uint8_t(min(255, b[1] + boost));
            *out++ = uint8_t(min(255, b[2] + boost));
        }
    }

    // Seaweed, swaying more towards its tip.
    for (const Weed &w : weeds)
    {
        for (int h = 0; h < w.height; h++)
        {
            const float up = float(h) / w.height;
            const float sway = up * up * 3.5f * sinAt(t * 25.0f + w.phase + h * 4.0f);
            const int sx = w.x + int(lroundf(sway));
            const int sy = STRIP_H - SAND_ROWS + 1 - h;
            drawStrip(sx, sy, w.color);
            if (h < w.height - 3)
            {
                drawStrip(sx + 1, sy, color::scale(w.color, 170));
            }
        }
    }

    // Fish, turning round at the ends of the tank.
    for (Fish &f : fish)
    {
        f.x += f.dir * f.speed * dt;
        const float half = SPRITES[f.kind].w / 2.0f;
        if ((f.dir < 0 && f.x < half) || (f.dir > 0 && f.x > STRIP_W - half))
        {
            f.dir = -f.dir;
            f.speed = 6 + frand() * 9;
            f.y = constrain(f.y + (frand() - 0.5f) * 12, 6.0f, float(STRIP_H - SAND_ROWS - 6));
        }
        drawFish(f);
    }

    // Bubbles: from the sand near the weeds, wobbling up, over the top edge.
    bubbleDebt += BUBBLES_PER_S * dt;
    for (Bubble &b : bubbles)
    {
        if (!b.alive)
        {
            if (bubbleDebt >= 1)
            {
                const Weed &w = weeds[random() % WEEDS];
                const int sx = constrain(w.x + int(random() % 5) - 2, 0, STRIP_W - 1);
                b.p = cube::toCube({int16_t(cube::CHAIN_WIDTH - 1 - sx), int16_t(SAND_ROWS)});
                b.v = {0, 0, 9.0f + frand() * 7.0f};
                b.wobble = frand() * 256;
                b.alive = true;
                bubbleDebt -= 1;
            }
            continue;
        }
        const int before = particles::faceAxis(b.p);
        // Wobble along the face.
        const float w = 3.0f * sinAt(t * 60.0f + b.wobble);
        if (before == 0)
        {
            b.v.y = w;
        }
        else if (before == 1)
        {
            b.v.x = w;
        }
        if (!particles::move(b.p, b.v, RISE, dt))
        {
            b.alive = false;
            continue;
        }
        if (particles::faceAxis(b.p) == 2)
        {
            // Reached the surface: pop into a ripple.
            for (Ripple &r : ripples)
            {
                if (!r.alive)
                {
                    r = {b.p.x, b.p.y, 0, true};
                    break;
                }
            }
            b.alive = false;
            continue;
        }
        const cube::Point p = cube::fromCube(b.p);
        if (p.valid())
        {
            canvas.drawPixelRGB888(p.x, p.y, 150, 220, 255);
        }
    }
    bubbleDebt = min(bubbleDebt, 3.0f);

    for (Ripple &r : ripples)
    {
        r.age += dt;
    }
    drawTop(t);
}
