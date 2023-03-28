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

void GameOfLife::show()
{
    // initialize the pixels
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            currentFrame[x][y] = random(0, 3) == 0;
        }
    }

    while (true)
    {
        // draw the pixels
        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                if (currentFrame[x][y])
                {
                  pattern->display->drawPixelRGB888(x, y, 255, 255, 255);
                } else {
                  pattern->display->drawPixelRGB888(x, y, 0, 0, 0);
                }
            }
        }

        // update the pixels
        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                int neighbors = 0;
                for (int i = -1; i <= 1; i++)
                {
                    for (int j = -1; j <= 1; j++)
                    {
                        if (i == 0 && j == 0)
                        {
                            continue;
                        }
                        if (currentFrame[(x + i + 64) % 64][(y + j + 64) % 64])
                        {
                            neighbors++;
                        }
                    }
                }
                if (currentFrame[x][y])
                {
                    if (neighbors < 2 || neighbors > 3)
                    {
                        nextFrame[x][y] = false;
                    }
                }
                else
                {
                    if (neighbors == 3)
                    {
                        nextFrame[x][y] = true;
                    }
                }
            }
        }

        // set current frame to next frame
        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                currentFrame[x][y] = nextFrame[x][y];
            }
        }


        // wait for the next frame
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}