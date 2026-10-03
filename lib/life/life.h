#ifndef LIFE_H
#define LIFE_H

// Conway's Game of Life on a wrapping (toroidal) grid. Hardware independent,
// so it is unit tested on the host (test/test_life).

#include <stdint.h>

namespace life
{
    /// Number of live cells among the eight neighbours of (x, y), wrapping at
    /// the grid edges. Grids are row-major, one byte per cell, non-zero = alive.
    int neighbours(const uint8_t *grid, int width, int height, int x, int y);

    /**
     * Writes the generation after `current` into `next`. Every cell of `next`
     * is written, so it does not need clearing first and may hold anything.
     * `current` and `next` must not overlap.
     *
     * @return the number of live cells in `next`.
     */
    int step(const uint8_t *current, uint8_t *next, int width, int height);

    /**
     * The same rules on an arbitrary neighbourhood graph -- e.g. the cube
     * surface from cube::buildNeighbours(), where cells near a seam have
     * neighbours on another face.
     *
     * @param neighbours cells * 8 cell indices; -1 for "no neighbour".
     * @return the number of live cells in `next`.
     */
    int stepGraph(const uint8_t *current, uint8_t *next, int cells, const int16_t *neighbours);
}

#endif
