#ifndef SAND_H
#define SAND_H

// Falling sand on a grid with gravity towards row 0. Hardware independent
// (test/test_sand).

#include <stdint.h>

namespace sand
{
    /**
     * Moves every grain at most one cell: straight down if that is empty,
     * otherwise diagonally down to whichever side is empty (both empty: the
     * side picked by `random`). Columns 0 and width-1 are walls. Rows are
     * processed bottom-up so a grain falls once per step.
     *
     * @param grid width * height, row-major, row 0 the bottom. 0 = empty;
     * anything else is a grain (its value is kept, e.g. a colour).
     * @param random Any source of random bits.
     * @return the number of grains that moved; 0 once everything has settled.
     */
    int step(uint8_t *grid, int width, int height, uint32_t (*random)());

    /// Grains on the grid.
    int count(const uint8_t *grid, int cells);
}

#endif
