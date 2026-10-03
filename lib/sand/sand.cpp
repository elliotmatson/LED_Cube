#include "sand.h"

namespace sand
{
    int step(uint8_t *grid, int width, int height, uint32_t (*random)())
    {
        int moved = 0;
        for (int y = 1; y < height; y++)
        {
            uint8_t *row = grid + y * width;
            uint8_t *below = row - width;
            // Alternate the sweep direction by row so piles do not lean.
            const bool leftToRight = (y & 1) != 0;
            for (int i = 0; i < width; i++)
            {
                const int x = leftToRight ? i : width - 1 - i;
                if (row[x] == 0)
                {
                    continue;
                }
                int target = -1;
                if (below[x] == 0)
                {
                    target = x;
                }
                else
                {
                    const bool left = x > 0 && below[x - 1] == 0;
                    const bool right = x < width - 1 && below[x + 1] == 0;
                    if (left && right)
                    {
                        target = (random() & 1) ? x - 1 : x + 1;
                    }
                    else if (left)
                    {
                        target = x - 1;
                    }
                    else if (right)
                    {
                        target = x + 1;
                    }
                }
                if (target >= 0)
                {
                    below[target] = row[x];
                    row[x] = 0;
                    moved++;
                }
            }
        }
        return moved;
    }

    int count(const uint8_t *grid, int cells)
    {
        int n = 0;
        for (int i = 0; i < cells; i++)
        {
            n += grid[i] != 0;
        }
        return n;
    }
}
