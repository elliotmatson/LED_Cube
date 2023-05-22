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
    top = new SinglePanel(*pattern->display, 0, 0);
    bottom = new BottomPanels(*pattern->display);
    frameCount = 0;
}

void GameOfLife::start()
{
    xTaskCreate(
        [](void *o)
        { static_cast<GameOfLife *>(o)->show(); }, // This is disgusting, but it works
        "GameOfLife - Refresh",                    // Name of the task (for debugging)
        6000,                                      // Stack size (bytes)
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
    initFrame();

    while (true)
    {
        // update the pixels
        drawCurrentFrame();

        // calculate the next frame
        calcNextFrame();
        vTaskDelay(20 / portTICK_PERIOD_MS);

        // draw intermediate frame
        drawIntermediateFrame();

        // set current frame to next frame
        for (int x = 0; x < 192; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                currentFrame[x][y] = nextFrame[x][y];
            }
        }
        frameCount = (frameCount + 1) % 8;

        // wait for the next frame
        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
}

void GameOfLife::initFrame()
{
    for (int x = 0; x < 192; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            currentFrame[x][y] = random(0, 5) == 0;
        }
    }
}

void GameOfLife::drawCurrentFrame()
{
    for (int x = 0; x < 192; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            if (x < 128)
            {
                if (currentFrame[x][y])
                {
                    bottom->drawPixelRGB888(x, y, 255, 255, 255);
                }
                else
                {
                    bottom->drawPixelRGB888(x, y, 0, 0, 0);
                }
            }
            else
            {
                if (currentFrame[x][y])
                {
                    top->drawPixelRGB888(x - 128, y, 255, 255, 255);
                }
                else
                {
                    top->drawPixelRGB888(x - 128, y, 0, 0, 0);
                }
            }
        }
    }
}

void GameOfLife::drawIntermediateFrame()
{
    for (int x = 0; x < 192; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            if (currentFrame[x][y] == 1 && nextFrame[x][y] == 0)
            {
                if (x < 128)
                {
                    bottom->drawPixelRGB888(x, y, 160, 160, 160);
                }
                else
                {
                    top->drawPixelRGB888(x - 128, y, 160, 160, 160);
                }
            }
            else if (currentFrame[x][y] == 0 && nextFrame[x][y] == 1)
            {
                if (x < 128)
                {
                    bottom->drawPixelRGB888(x, y, 160, 160, 160);
                }
                else
                {
                    top->drawPixelRGB888(x - 128, y, 160, 160, 160);
                }
            }
        }
    }
}

void GameOfLife::calcNextFrame()
{
    for (int x = 0; x < 192; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            int neighbors = calcNeighbors(x, y);
            
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
}

int GameOfLife::calcNeighbors(int x, int y)
{
    int neighbors = 0;
    int panel = x / 64;
    x = x % 64;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0)
            {
                continue;
            }
            int s = x + i;
            int t = y + j;
            //check (64,-1)

            switch (panel)
            {
            case 0:
                if (s < 0 || t >= 64) // off left or bottom edge
                {
                    continue;
                }
                if (s < 64 && t >= 0) // on the panel
                {
                    if (currentFrame[s][t])
                    {
                        neighbors++;
                    }
                }
                else if (s >= 64 && t >= 0) // on the panel to the right
                {
                    if (currentFrame[s][t])
                    {
                        neighbors++;
                    }
                }
                else if (s < 64 && t < 0) // on the panel above
                {
                    if (currentFrame[s + 128][t+64])
                    {
                        neighbors++;
                    }
                }
                break;
            case 1:
                if (s >= 64 || t >= 64) // off right or bottom edge
                {
                    continue;
                }
                if (s >= 0 && t >= 0) // on the panel
                {
                    if (currentFrame[s + 64][t])
                    {
                        neighbors++;
                    }
                }
                else if (s < 0 && t >= 0) // on the panel to the left
                {
                    if (currentFrame[s + 64][t])
                    {
                        neighbors++;
                    }
                }
                else if (s >= 0 && t < 0) // on the panel above
                {
                    if (currentFrame[192 + t][63 - s])
                    {
                        neighbors++;
                    }
                }
                break;
            case 2:
                if (s < 0 || t < 0) // off left or top edge
                {
                    continue;
                }
                if (s < 64 && t < 64) // on the panel
                {
                    if (currentFrame[s + 128][t])
                    {
                        neighbors++;
                    }
                }
                else if (s >= 64 && t < 64) // on the panel to the right 
                {
                    if (currentFrame[127-t][s-65])
                    {
                        neighbors++;
                    }
                }
                else if (s < 64 && t >= 64) // on the panel below
                {
                    if (currentFrame[s][t - 64])
                    {
                        neighbors++;
                    }
                }
                break;
            }
        }
    }
    return neighbors;
}