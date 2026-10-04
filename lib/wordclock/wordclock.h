#ifndef WORDCLOCK_H
#define WORDCLOCK_H

// A word clock: the time to the nearest five minutes, spelled out by
// lighting words in a fixed 16 x 8 grid of letters ("IT IS TWENTY FIVE
// MINUTES PAST NINE PM"). Hardware independent (test/test_wordclock).

#include <stdint.h>

namespace wordclock
{
    static const int COLS = 16;
    static const int ROWS = 8;

    /// The letters, row by row. Letters between words are filler.
    extern const char *const GRID[ROWS];

    /**
     * Marks the letters to light for `hour` (0-23) and `minute` (0-59):
     * lit[row * COLS + col] is 1 for a lit letter, 0 otherwise. Minutes are
     * rounded down to five; the extra minutes are for the caller to show.
     */
    void light(int hour, int minute, uint8_t *lit);

    /// The lit words, in reading order, separated by spaces -- for tests and
    /// logs. Returns `out`.
    char *phrase(int hour, int minute, char *out, int size);
}

#endif
