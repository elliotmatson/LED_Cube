#include "game_of_life.h"

GameOfLife::GameOfLife()
{
    data.id = "game_of_life";
    data.name = "Game of Life";
}

GameOfLife::~GameOfLife()
{
    end();
}

void GameOfLife::begin(PatternServices *services)
{
    pattern = services;
    current = static_cast<uint8_t *>(heap_caps_malloc(cube::CELLS, MALLOC_CAP_SPIRAM));
    next = static_cast<uint8_t *>(heap_caps_malloc(cube::CELLS, MALLOC_CAP_SPIRAM));
    neighbours = static_cast<int16_t *>(heap_caps_malloc(cube::CELLS * cube::NEIGHBOURS * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    if (!current || !next || !neighbours)
    {
        ESP_LOGE("GameOfLife", "Out of PSRAM");
        end();
        return;
    }
    cube::buildNeighbours(neighbours);
    lastPopulation = -1;
    unchanged = 0;
    seed();
    // Everything is drawn from here on as it changes.
    for (int cell = 0; cell < cube::CELLS; cell++)
    {
        uint8_t v = current[cell] ? 255 : 0;
        pattern->display->drawPixelRGB888(cell % cube::CHAIN_WIDTH, cell / cube::CHAIN_WIDTH, v, v, v);
    }
}

void GameOfLife::end()
{
    free(current);
    free(next);
    free(neighbours);
    current = next = nullptr;
    neighbours = nullptr;
}

void GameOfLife::seed()
{
    for (int i = 0; i < cube::CELLS; i++)
    {
        current[i] = random(0, 3) == 0;
    }
}

void GameOfLife::tick()
{
    if (!current)
    {
        return;
    }
    // A population that has not changed for this many generations has
    // settled into still lifes and blinkers; start again.
    const int STALE_GENERATIONS = 100;

    int population = life::stepGraph(current, next, cube::CELLS, neighbours);
    // Only cells that changed are drawn, which keeps the push small once the
    // first chaotic generations are over.
    for (int cell = 0; cell < cube::CELLS; cell++)
    {
        if (next[cell] != current[cell])
        {
            uint8_t v = next[cell] ? 255 : 0;
            pattern->display->drawPixelRGB888(cell % cube::CHAIN_WIDTH, cell / cube::CHAIN_WIDTH, v, v, v);
        }
    }
    uint8_t *t = current;
    current = next;
    next = t;

    unchanged = (population == lastPopulation) ? unchanged + 1 : 0;
    lastPopulation = population;
    if (population == 0 || unchanged >= STALE_GENERATIONS)
    {
        seed();
        unchanged = 0;
        pattern->display->fillScreen(0);
        for (int cell = 0; cell < cube::CELLS; cell++)
        {
            if (current[cell])
            {
                pattern->display->drawPixelRGB888(cell % cube::CHAIN_WIDTH, cell / cube::CHAIN_WIDTH, 255, 255, 255);
            }
        }
    }
}
