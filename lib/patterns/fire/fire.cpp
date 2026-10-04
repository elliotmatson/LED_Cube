#include "fire.h"

#include <math.h>
#include "particles.h"

namespace
{
    // Heat lost per (half-resolution) row, at most: sets the flames' height.
    const int COOLING = 22;
    const float EMBERS_PER_S = 9.0f;
    const cube::Vec3 BUOYANCY = {0, 0, 6}; // embers keep rising up the sides
    const float DRAG = 0.7f;               // per second, mostly felt on the top

    const color::RGB HEAT_STOPS[] = {
        {0, 0, 0},
        {70, 0, 0},
        {190, 20, 0},
        {255, 90, 0},
        {255, 180, 30},
        {255, 240, 160},
    };

    float frand(uint32_t r) { return (r & 0xFFFF) / 65535.0f; }
}

Fire::Fire()
{
    data.id = "fire";
    data.name = "Fire";
}

Fire::~Fire()
{
    end();
}

uint32_t Fire::random()
{
    // xorshift32: plenty for flicker, and much cheaper than esp_random().
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

void Fire::begin(PatternServices *services)
{
    pattern = services;
    heat = static_cast<uint8_t *>(heap_caps_calloc(SIM_W * SIM_H, 1, MALLOC_CAP_8BIT));
    if (heat == nullptr)
    {
        return;
    }
    for (int i = 0; i < 256; i++)
    {
        palette[i] = color::gradient(HEAT_STOPS, 6, uint8_t(i));
    }
    for (Ember &e : embers)
    {
        e.age = e.life = 0;
    }
    memset(columnGlow, 0, sizeof(columnGlow));
    rng = esp_random() | 1;
    lastMs = millis();
    spawnDebt = 0;
}

void Fire::end()
{
    free(heat);
    heat = nullptr;
}

void Fire::spawnEmber(Ember &e)
{
    // From a hot column, around where the flames thin out.
    int sx = 0, sy = 0;
    for (int tries = 0; tries < 8; tries++)
    {
        sx = int(random() % STRIP_W);
        sy = 14 + int(random() % 20);
        if (heat[(sy / 2) * SIM_W + sx / 2] > 120)
        {
            break;
        }
    }
    // Strip (sx, sy) is chain (191 - sx, 63 - sy).
    e.p = cube::toCube({int16_t(cube::CHAIN_WIDTH - 1 - sx), int16_t(STRIP_H - 1 - sy)});
    // Up, with a little sideways drift along whichever side face it is on.
    const float side = (frand(random()) - 0.5f) * 8.0f;
    e.v = {0, 0, 10.0f + frand(random()) * 10.0f};
    if (particles::faceAxis(e.p) == 0)
    {
        e.v.y = side;
    }
    else
    {
        e.v.x = side;
    }
    e.age = 0;
    e.life = 2.0f + frand(random()) * 2.5f;
}

void Fire::tick()
{
    if (heat == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;

    // Feed the bottom row: hot, flickering.
    uint8_t *bottom = heat + (SIM_H - 1) * SIM_W;
    for (int x = 0; x < SIM_W; x++)
    {
        bottom[x] = uint8_t(190 + random() % 66);
    }
    // Rise (the Doom fire): each cell's heat moves up a row, a cell to
    // either side or straight, losing a random amount. The sideways jitter
    // is what makes tongues of flame. Top down, so each row read is still
    // last frame's.
    for (int y = 0; y < SIM_H - 1; y++)
    {
        const uint8_t *below = heat + (y + 1) * SIM_W;
        uint8_t *row = heat + y * SIM_W;
        for (int x = 0; x < SIM_W; x++)
        {
            const uint32_t r = random();
            int to = x + int(r % 3) - 1;
            to = to < 0 ? 0 : (to >= SIM_W ? SIM_W - 1 : to);
            const int cool = int((r >> 8) % COOLING);
            row[to] = uint8_t(below[x] > cool ? below[x] - cool : 0);
        }
    }

    // How lit the top of each column is, for firelight on the top face.
    for (int x = 0; x < SIM_W; x++)
    {
        int lit = 0;
        for (int y = 0; y < SIM_H * 2 / 3; y += 2)
        {
            lit = max<int>(lit, heat[y * SIM_W + x]);
        }
        columnGlow[x] = uint8_t((columnGlow[x] * 3 + lit) / 4);
    }

    Canvas &canvas = *pattern->display;
    // The top face: firelight near the two edges it shares with the sides.
    // Face 1 runs along the top's x = 64 edge, its column y is strip x
    // 127 - y; face 2 along y = 64, strip x = x.
    for (int16_t y = 0; y < cube::FACE_SIZE; y++)
    {
        uint8_t *out = canvas.rowForWrite(y, 0, cube::FACE_SIZE);
        for (int16_t x = 0; x < cube::FACE_SIZE; x++)
        {
            const int d1 = cube::FACE_SIZE - 1 - x, d2 = cube::FACE_SIZE - 1 - y;
            int glow = 0;
            if (d1 < 12)
            {
                glow += columnGlow[(127 - y) / 2] * (12 - d1) / 12;
            }
            if (d2 < 12)
            {
                glow += columnGlow[x / 2] * (12 - d2) / 12;
            }
            glow = min(glow / 5, 60);
            *out++ = uint8_t(glow);
            *out++ = uint8_t(glow / 5);
            *out++ = 0;
        }
    }
    // The sides, scaled up from the simulation with bilinear smoothing.
    // Chain x 64-191 is strip x 127-0, chain row 63 - strip row.
    for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
    {
        uint8_t *out = canvas.rowForWrite(row, cube::FACE_SIZE, STRIP_W);
        const int sy = STRIP_H - 1 - row;
        // Sample centres sit at 2 * cell + 0.5: strip row sy is between
        // cells (sy - 0.5) / 2 and the next.
        const int fy4 = max(0, sy * 2 - 1); // in quarter cells
        const int cy0 = min(fy4 / 4, SIM_H - 1), cy1 = min(cy0 + 1, SIM_H - 1);
        const int wy = fy4 & 3;
        const uint8_t *r0 = heat + cy0 * SIM_W, *r1 = heat + cy1 * SIM_W;
        for (int sx = STRIP_W - 1; sx >= 0; sx--)
        {
            const int fx4 = max(0, sx * 2 - 1);
            const int cx0 = min(fx4 / 4, SIM_W - 1), cx1 = min(cx0 + 1, SIM_W - 1);
            const int wx = fx4 & 3;
            const int top = r0[cx0] * (4 - wx) + r0[cx1] * wx;
            const int bot = r1[cx0] * (4 - wx) + r1[cx1] * wx;
            const color::RGB &c = palette[(top * (4 - wy) + bot * wy) >> 4];
            *out++ = c.r;
            *out++ = c.g;
            *out++ = c.b;
        }
    }

    // Embers: born at the flame tips, rising over the edge, fading.
    spawnDebt += EMBERS_PER_S * dt;
    for (Ember &e : embers)
    {
        if (e.age >= e.life)
        {
            if (spawnDebt >= 1)
            {
                spawnEmber(e);
                spawnDebt -= 1;
            }
            continue;
        }
        e.age += dt;
        // A little turbulence, and drag.
        e.v.x += (frand(random()) - 0.5f) * 30.0f * dt;
        e.v.y += (frand(random()) - 0.5f) * 30.0f * dt;
        const float keep = 1.0f - DRAG * dt;
        e.v.x *= keep;
        e.v.y *= keep;
        if (!particles::move(e.p, e.v, BUOYANCY, dt))
        {
            e.age = e.life;
            continue;
        }
        const cube::Point p = cube::fromCube(e.p);
        if (!p.valid())
        {
            continue;
        }
        // Yellow-hot, cooling through orange to a dull red.
        const float k = e.age / e.life;
        const color::RGB c = color::lerp({255, 220, 80}, {120, 10, 0}, uint8_t(k * 255));
        const float level = 1.0f - k * k;
        canvas.drawPixelRGB888(p.x, p.y, uint8_t(c.r * level), uint8_t(c.g * level), uint8_t(c.b * level));
    }
    spawnDebt = min(spawnDebt, 3.0f);
}
