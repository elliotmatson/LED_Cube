#include "game_of_life.h"

// Game of life pattern
// Based on
//

GameOfLife::GameOfLife()
{
    data.name = "Game of Life";
}

GameOfLife::~GameOfLife()
{
    stop();
}

void GameOfLife::init(PatternServices *pattern)
{
    this->pattern = pattern;
    frameCount = 0;
}

void GameOfLife::start()
{
    xTaskCreate(
        [](void *o)
        { static_cast<GameOfLife *>(o)->show(); }, // This is disgusting, but it works
        "GameOfLife - Refresh",                    // Name of the task (for debugging)
        4000,                                      // Stack size (bytes)
        this,                                      // Parameter to pass
        1,                                         // Task priority
        &refreshTask                               // Task handle
    );
}

void GameOfLife::stop()
{
    if (refreshTask)
    {
        vTaskDelete(refreshTask);
        refreshTask = NULL;
    }
}

void GameOfLife::seed()
{
    for (int i = 0; i < 64 * 64; i++)
    {
        currentFrame[i] = random(0, 3) == 0;
    }
}

void GameOfLife::show()
{
    // A population that has not changed for this many generations has
    // settled into still lifes and blinkers; start again.
    const int STALE_GENERATIONS = 100;
    int lastPopulation = -1;
    int unchanged = 0;

    seed();
    while (true)
    {
        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                uint8_t v = currentFrame[y * 64 + x] ? 255 : 0;
                pattern->display->drawPixelRGB888(x, y, v, v, v);
            }
        }

        int population = life::step(currentFrame, nextFrame, 64, 64);
        memcpy(currentFrame, nextFrame, sizeof(currentFrame));

        unchanged = (population == lastPopulation) ? unchanged + 1 : 0;
        lastPopulation = population;
        if (population == 0 || unchanged >= STALE_GENERATIONS)
        {
            seed();
            unchanged = 0;
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
