#ifndef LANGTON_H
#define LANGTON_H

// Langton's ant and its multi-colour generalisations (turmites), on any
// graph of cells with four neighbours each whose directions may be local to
// each cell, such as blocks across the cube's seams
// (cube::buildBlockNeighbours). Hardware independent (test/test_langton).

#include <stdint.h>

namespace langton
{
    static const int MAX_STATES = 16;

    /// One turn per cell state: 'L' left, 'R' right, 'N' straight on,
    /// 'U' about turn. Each visit advances the cell to the next state.
    struct Rule
    {
        char turns[MAX_STATES];
        uint8_t states = 0;
    };

    /// Parses a rule such as "RL" (the classic ant) or "LLRR". False if it
    /// is empty, too long or has other letters.
    bool parse(const char *text, Rule &rule);

    struct Ant
    {
        int16_t cell;
        uint8_t dir; // 0 up, 1 right, 2 down, 3 left, in its cell's frame
    };

    /**
     * One step: turn by the rule for the ant's cell, advance that cell's
     * state, and move one cell on. Crossing to a cell whose directions are
     * rotated (a seam) turns the heading with it; at a wall (neighbour -1)
     * the ant turns round instead of moving.
     *
     * @param nbr cells * 4 neighbours, -1 for none.
     * @return the cell whose state changed.
     */
    int step(const Rule &rule, uint8_t *grid, const int16_t *nbr, Ant &ant);
}

#endif
