#include "maze_pattern.h"

namespace
{
    const int GROW_PER_FRAME = 5;
    const int SOLVE_PER_FRAME = 2;
    const uint32_t HOLD_MS = 5000;
    const uint32_t FADE_MS = 1500;
    const int ACROSS = cube::CHAIN_WIDTH / MazePattern::BLOCK;
}

MazePattern::MazePattern()
{
    data.id = "maze";
    data.name = "Maze";
}

MazePattern::~MazePattern()
{
    end();
}

void MazePattern::begin(PatternServices *services)
{
    pattern = services;
    cells = cube::blockCount(BLOCK);
    nbr = static_cast<int16_t *>(heap_caps_malloc(cells * 4 * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    open = static_cast<uint8_t *>(heap_caps_malloc(cells, MALLOC_CAP_SPIRAM));
    revealed = static_cast<uint8_t *>(heap_caps_malloc(cells, MALLOC_CAP_SPIRAM));
    visited = static_cast<uint8_t *>(heap_caps_malloc(cells, MALLOC_CAP_SPIRAM));
    events = static_cast<maze::Carve *>(heap_caps_malloc(cells * sizeof(maze::Carve), MALLOC_CAP_SPIRAM));
    scratchA = static_cast<int16_t *>(heap_caps_malloc(cells * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    scratchB = static_cast<int16_t *>(heap_caps_malloc(cells * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    scratchC = static_cast<int16_t *>(heap_caps_malloc(cells * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    path = static_cast<int16_t *>(heap_caps_malloc(cells * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    if (!nbr || !open || !revealed || !visited || !events || !scratchA || !scratchB || !scratchC || !path)
    {
        end();
        return;
    }
    cube::buildBlockNeighbours(BLOCK, nbr);
    seed = esp_random() | 1;
    hue = uint8_t(esp_random());
    restart();
}

void MazePattern::end()
{
    free(nbr);
    free(open);
    free(revealed);
    free(visited);
    free(events);
    free(scratchA);
    free(scratchB);
    free(scratchC);
    free(path);
    nbr = nullptr;
    open = revealed = visited = nullptr;
    events = nullptr;
    scratchA = scratchB = scratchC = path = nullptr;
}

void MazePattern::restart()
{
    pattern->display->fillScreen(0);
    const int start = int(seed % cells);
    eventCount = maze::generate(nbr, cells, start, open, events, scratchA, visited, seed);
    // The route: from the start to the cell farthest from it.
    const int goal = maze::search(nbr, open, cells, start, scratchA, scratchB, scratchC);
    pathLength = maze::route(scratchB, goal, path, cells);
    memset(revealed, 0, cells);
    shown = pathShown = 0;
    head = start;
    hue += 70;
    phase = Phase::GROW;
    phaseStartMs = millis();
}

/// A cell's 2x2 middle and the openings in `mask`. Blocks have a one-pixel
/// border on every side, so a corridor between two blocks is both their
/// borders, whichever way the seams turn them.
void MazePattern::drawCell(int cell, uint8_t mask, color::RGB c)
{
    const int16_t x0 = int16_t((cell % ACROSS) * BLOCK), y0 = int16_t((cell / ACROSS) * BLOCK);
    Canvas &canvas = *pattern->display;
    for (int16_t j = 1; j <= 2; j++)
    {
        for (int16_t i = 1; i <= 2; i++)
        {
            canvas.drawPixelRGB888(x0 + i, y0 + j, c.r, c.g, c.b);
        }
    }
    for (int d = 0; d < 4; d++)
    {
        if (mask & (1 << d))
        {
            drawOpening(cell, d, c);
        }
    }
}

void MazePattern::drawOpening(int cell, int dir, color::RGB c)
{
    const int16_t x0 = int16_t((cell % ACROSS) * BLOCK), y0 = int16_t((cell / ACROSS) * BLOCK);
    Canvas &canvas = *pattern->display;
    for (int16_t k = 1; k <= 2; k++)
    {
        switch (dir)
        {
        case cube::UP: canvas.drawPixelRGB888(x0 + k, y0, c.r, c.g, c.b); break;
        case cube::RIGHT: canvas.drawPixelRGB888(x0 + 3, y0 + k, c.r, c.g, c.b); break;
        case cube::DOWN: canvas.drawPixelRGB888(x0 + k, y0 + 3, c.r, c.g, c.b); break;
        default: canvas.drawPixelRGB888(x0, y0 + k, c.r, c.g, c.b); break;
        }
    }
}

color::RGB MazePattern::routeColor(int i) const
{
    // Complementary to the walls' hue, sweeping a third of the wheel.
    return color::hsv(uint8_t(hue + 128 + i * 85 / max(pathLength, 1)), 255, 255);
}

/// Route cells [0, upTo), with the openings between consecutive ones.
void MazePattern::drawRoute(int upTo, uint8_t level)
{
    for (int i = 0; i < upTo; i++)
    {
        const color::RGB c = color::scale(routeColor(i), level);
        drawCell(path[i], 0, c);
        if (i + 1 < upTo)
        {
            for (int d = 0; d < 4; d++)
            {
                if (nbr[path[i] * 4 + d] == path[i + 1])
                {
                    drawOpening(path[i], d, c);
                }
                if (nbr[path[i + 1] * 4 + d] == path[i])
                {
                    drawOpening(path[i + 1], d, c);
                }
            }
        }
    }
}

void MazePattern::tick()
{
    if (nbr == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const color::RGB corridor = color::hsv(hue, 190, 110);
    switch (phase)
    {
    case Phase::GROW:
        for (int k = 0; k < GROW_PER_FRAME && shown < eventCount; k++, shown++)
        {
            const maze::Carve &e = events[shown];
            const int to = nbr[e.cell * 4 + e.dir];
            revealed[e.cell] |= uint8_t(1 << e.dir);
            for (int d = 0; d < 4; d++)
            {
                if (nbr[to * 4 + d] == e.cell)
                {
                    revealed[to] |= uint8_t(1 << d);
                }
            }
            if (head >= 0)
            {
                drawCell(head, revealed[head], corridor);
            }
            drawCell(e.cell, revealed[e.cell], corridor);
            head = to;
        }
        if (head >= 0)
        {
            drawCell(head, revealed[head], {255, 255, 255});
        }
        if (shown >= eventCount)
        {
            if (head >= 0)
            {
                drawCell(head, revealed[head], corridor);
            }
            head = -1;
            phase = Phase::SOLVE;
            phaseStartMs = now;
        }
        break;
    case Phase::SOLVE:
        pathShown = min(pathLength, pathShown + SOLVE_PER_FRAME);
        drawRoute(pathShown, 255);
        if (pathShown >= pathLength)
        {
            phase = Phase::HOLD;
            phaseStartMs = now;
        }
        break;
    case Phase::HOLD:
    {
        // The route pulses gently.
        const float t = (now - phaseStartMs) / 1000.0f;
        drawRoute(pathLength, uint8_t(170 + 85 * (0.5f + 0.5f * sinf(t * 4.0f))));
        if (now - phaseStartMs >= HOLD_MS)
        {
            phase = Phase::FADE;
            phaseStartMs = now;
        }
        break;
    }
    case Phase::FADE:
    {
        const uint32_t t = now - phaseStartMs;
        if (t >= FADE_MS)
        {
            restart();
            break;
        }
        const uint8_t level = uint8_t(255 - t * 255 / FADE_MS);
        const color::RGB c = color::scale(corridor, level);
        for (int i = 0; i < cells; i++)
        {
            drawCell(i, revealed[i], c);
        }
        drawRoute(pathLength, level);
        break;
    }
    }
}
