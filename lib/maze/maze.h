#ifndef MAZE_H
#define MAZE_H

// Mazes on any graph of cells with up to four neighbours each, such as the
// cube's surface (cube::buildBlockNeighbours). Hardware independent
// (test/test_maze). The caller provides every buffer.

#include <stdint.h>

namespace maze
{
    /// A passage opened from `cell` in direction `dir`.
    struct Carve
    {
        int16_t cell;
        uint8_t dir;
    };

    /**
     * Carves a perfect maze (a spanning tree: exactly one route between any
     * two cells) by randomised depth-first search from `start`, which gives
     * long, winding corridors.
     *
     * @param nbr cells * 4 neighbours, -1 for none.
     * @param open Out: per cell, bit d set when its passage in direction d is
     * open. Both ends of a passage are marked.
     * @param events Out: the passages in the order they were carved, for
     * drawing the maze growing (cells reachable - 1 of them).
     * @param stack, visited Scratch, `cells` entries each.
     * @param seed Random state, any non-zero value; advanced.
     * @return the number of events.
     */
    int generate(const int16_t *nbr, int cells, int start, uint8_t *open, Carve *events, int16_t *stack,
                 uint8_t *visited, uint32_t &seed);

    /**
     * Breadth-first search through the open passages from `start`.
     *
     * @param dist Out: steps from start, -1 if unreachable.
     * @param prev Out: the cell each was reached from, -1 for start.
     * @param queue Scratch, `cells` entries.
     * @return the farthest reachable cell.
     */
    int search(const int16_t *nbr, const uint8_t *open, int cells, int start, int16_t *dist, int16_t *prev,
               int16_t *queue);

    /// The route from search()'s start to `goal`, start first, into `out`;
    /// returns its length in cells.
    int route(const int16_t *prev, int goal, int16_t *out, int capacity);
}

#endif
