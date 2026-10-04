#include "langtons_ant.h"

namespace
{
    // Rules that grow into something worth watching: the classic highway,
    // symmetric blooms, square spirals and chaotic textures.
    const char *const RULES[] = {
        "RL", "RLR", "LLRR", "LRRRRRLLR", "RRLLLRLLLRRR", "LLRRRLRLRLLR", "RRLRLLRRRRRR", "LRRRRLLLRRR",
    };
    const int RULE_COUNT = sizeof(RULES) / sizeof(RULES[0]);
    const uint32_t RUN_MS = 3 * 60 * 1000;
    const uint32_t FADE_MS = 1500;
    const int ACROSS = cube::CHAIN_WIDTH / LangtonsAnt::BLOCK;
}

LangtonsAnt::LangtonsAnt()
{
    data.id = "langtons_ant";
    data.name = "Langton's Ant";
}

LangtonsAnt::~LangtonsAnt()
{
    end();
}

void LangtonsAnt::begin(PatternServices *services)
{
    pattern = services;
    cells = cube::blockCount(BLOCK);
    nbr = static_cast<int16_t *>(heap_caps_malloc(cells * 4 * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    grid = static_cast<uint8_t *>(heap_caps_malloc(cells, MALLOC_CAP_SPIRAM));
    if (nbr == nullptr || grid == nullptr)
    {
        end();
        return;
    }
    cube::buildBlockNeighbours(BLOCK, nbr);
    hue = uint8_t(esp_random());
    restart();
}

void LangtonsAnt::end()
{
    free(nbr);
    free(grid);
    nbr = nullptr;
    grid = nullptr;
}

void LangtonsAnt::restart()
{
    memset(grid, 0, cells);
    coloured = 0;
    langton::parse(RULES[esp_random() % RULE_COUNT], rule);
    ESP_LOGI("Langton", "Rule %.*s", rule.states, rule.turns);
    for (langton::Ant &a : ants)
    {
        a.cell = int16_t(esp_random() % cells);
        a.dir = uint8_t(esp_random() & 3);
    }
    hue += 85;
    fading = false;
    startMs = millis();
    pattern->display->fillScreen(0);
}

color::RGB LangtonsAnt::stateColor(uint8_t state, uint8_t level) const
{
    if (state == 0)
    {
        return {0, 0, 0};
    }
    // The states spread over two thirds of the wheel from this run's hue.
    const uint8_t h = uint8_t(hue + (state - 1) * 170 / max<int>(rule.states - 1, 1));
    return color::hsv(h, 230, uint8_t(170 * level / 255));
}

void LangtonsAnt::drawCell(int cell, color::RGB c)
{
    const int16_t x0 = int16_t((cell % ACROSS) * BLOCK), y0 = int16_t((cell / ACROSS) * BLOCK);
    Canvas &canvas = *pattern->display;
    for (int16_t j = 0; j < BLOCK; j++)
    {
        for (int16_t i = 0; i < BLOCK; i++)
        {
            canvas.drawPixelRGB888(x0 + i, y0 + j, c.r, c.g, c.b);
        }
    }
}

void LangtonsAnt::tick()
{
    if (grid == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    if (fading)
    {
        const uint32_t t = now - fadeStartMs;
        if (t >= FADE_MS)
        {
            restart();
            return;
        }
        const uint8_t level = uint8_t(255 - t * 255 / FADE_MS);
        for (int i = 0; i < cells; i++)
        {
            if (grid[i])
            {
                drawCell(i, stateColor(grid[i], level));
            }
        }
        return;
    }

    // Slow enough to follow at first, then faster: 2 steps a frame per ant,
    // rising to 60 over the first minute.
    const uint32_t age = now - startMs;
    const int steps = 2 + int(min<uint32_t>(age, 60000) * 58 / 60000);
    for (langton::Ant &a : ants)
    {
        for (int s = 0; s < steps; s++)
        {
            const int changed = langton::step(rule, grid, nbr, a);
            const uint8_t st = grid[changed];
            // States cycle; 0 is uncoloured.
            if (st == 0)
            {
                coloured--;
            }
            else if (st == 1)
            {
                coloured++;
            }
            drawCell(changed, stateColor(st));
        }
    }
    for (const langton::Ant &a : ants)
    {
        drawCell(a.cell, {255, 255, 255});
    }

    if (age >= RUN_MS || coloured > cells * 7 / 10)
    {
        // Put the ants' cells back to their colours before fading.
        for (const langton::Ant &a : ants)
        {
            drawCell(a.cell, stateColor(grid[a.cell]));
        }
        fading = true;
        fadeStartMs = now;
    }
}
