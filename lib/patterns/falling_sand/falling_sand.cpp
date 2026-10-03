#include "falling_sand.h"

namespace
{
    uint32_t rnd() { return esp_random(); }

    color::RGB grainColor(uint8_t value)
    {
        // value is hue + 1; warm, slightly desaturated, like coloured sand.
        return color::hsv(uint8_t(value - 1), 170, 230);
    }
}

FallingSand::FallingSand()
{
    data.id = "falling_sand";
    data.name = "Falling Sand";
}

FallingSand::~FallingSand()
{
    end();
}

void FallingSand::begin(PatternServices *services)
{
    pattern = services;
    grid = static_cast<uint8_t *>(heap_caps_calloc(GRID_W * GRID_H, 1, MALLOC_CAP_SPIRAM));
    for (Pour &p : pours)
    {
        p = {float(random(8, 48)), float(random(8, 48)), (random(2) ? 1 : -1) * 0.15f, (random(2) ? 1 : -1) * 0.11f};
    }
    for (Grain &g : grains)
    {
        g.active = false;
    }
    hue = uint8_t(random(256));
    draining = false;
    frame = 0;
}

void FallingSand::end()
{
    free(grid);
    grid = nullptr;
}

void FallingSand::tick()
{
    if (!grid)
    {
        return;
    }
    frame++;
    if (frame % 6 == 0)
    {
        hue++; // slow drift: layers of colour in the piles
    }

    // Pour points wander about the top face, bouncing off its sides.
    for (Pour &p : pours)
    {
        p.x += p.vx;
        p.y += p.vy;
        if (p.x < 4 || p.x > 56)
            p.vx = -p.vx;
        if (p.y < 4 || p.y > 56)
            p.vy = -p.vy;
    }

    // Each pour drops a grain every other frame, heading for one of the two
    // near edges (towards face 1 along +x, towards face 2 along +y).
    if (!draining && frame % 2 == 0)
    {
        for (const Pour &p : pours)
        {
            for (Grain &g : grains)
            {
                if (!g.active)
                {
                    g = {{int16_t(p.x), int16_t(p.y)}, random(2) ? cube::RIGHT : cube::DOWN, hue, true};
                    break;
                }
            }
        }
    }

    // Grains in flight move one cell a frame; on reaching a side face they
    // join the sand grid at its top row.
    for (Grain &g : grains)
    {
        if (!g.active)
        {
            continue;
        }
        cube::Step s = cube::step(g.at, g.dir);
        g.active = false;
        if (!s.valid())
        {
            continue;
        }
        if (cube::face(s.to.x) == 0)
        {
            g.at = s.to;
            g.dir = s.dir;
            g.active = true;
        }
        else
        {
            uint8_t &cell = grid[s.to.y * GRID_W + (s.to.x - cube::FACE_SIZE)];
            if (cell == 0)
            {
                cell = uint8_t(g.hue + 1);
            }
        }
    }

    sand::step(grid, GRID_W, GRID_H, rnd);

    // Nearly full: open the floor until it has all run out.
    if (!draining && sand::count(grid + (GRID_H - 6) * GRID_W, GRID_W) > GRID_W / 3)
    {
        draining = true;
    }
    if (draining)
    {
        memset(grid, 0, GRID_W); // the bottom row falls away
        if (sand::count(grid, GRID_W * GRID_H) == 0)
        {
            draining = false;
        }
    }

    // Top face: dark, with the pour points and grains in flight.
    pattern->display->fillRect(0, 0, cube::FACE_SIZE, cube::FACE_SIZE, 0);
    for (const Pour &p : pours)
    {
        pattern->display->fillRect(int16_t(p.x) - 1, int16_t(p.y) - 1, 3, 3, Canvas::color565(60, 60, 60));
    }
    for (const Grain &g : grains)
    {
        if (g.active)
        {
            color::RGB c = grainColor(uint8_t(g.hue + 1));
            pattern->display->drawPixelRGB888(g.at.x, g.at.y, c.r, c.g, c.b);
        }
    }

    // Side faces: the grid.
    for (int y = 0; y < GRID_H; y++)
    {
        uint8_t *out = pattern->display->rowForWrite(y, cube::FACE_SIZE, GRID_W);
        const uint8_t *row = grid + y * GRID_W;
        for (int x = 0; x < GRID_W; x++)
        {
            if (row[x])
            {
                color::RGB c = grainColor(row[x]);
                *out++ = c.r;
                *out++ = c.g;
                *out++ = c.b;
            }
            else
            {
                *out++ = 0;
                *out++ = 0;
                *out++ = 0;
            }
        }
    }
}
