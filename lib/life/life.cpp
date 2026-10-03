#include "life.h"

namespace life
{
    int neighbours(const uint8_t *grid, int width, int height, int x, int y)
    {
        int count = 0;
        for (int dy = -1; dy <= 1; dy++)
        {
            const int row = ((y + dy + height) % height) * width;
            for (int dx = -1; dx <= 1; dx++)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }
                if (grid[row + (x + dx + width) % width])
                {
                    count++;
                }
            }
        }
        return count;
    }

    int stepGraph(const uint8_t *current, uint8_t *next, int cells, const int16_t *neighbours)
    {
        int alive = 0;
        for (int cell = 0; cell < cells; cell++)
        {
            const int16_t *n = neighbours + cell * 8;
            int count = 0;
            for (int k = 0; k < 8; k++)
            {
                count += n[k] >= 0 && current[n[k]];
            }
            const bool isAlive = current[cell] ? (count == 2 || count == 3) : (count == 3);
            next[cell] = isAlive ? 1 : 0;
            alive += isAlive;
        }
        return alive;
    }

    int step(const uint8_t *current, uint8_t *next, int width, int height)
    {
        int alive = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const int n = neighbours(current, width, height, x, y);
                const bool wasAlive = current[y * width + x] != 0;
                // Survival with 2 or 3 neighbours, birth with exactly 3. Written
                // out for every cell: leaving survivors untouched is what made
                // the first generation read uninitialized memory.
                const bool isAlive = wasAlive ? (n == 2 || n == 3) : (n == 3);
                next[y * width + x] = isAlive ? 1 : 0;
                alive += isAlive;
            }
        }
        return alive;
    }
}
