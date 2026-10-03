#ifndef RUBIKS_H
#define RUBIKS_H

// A Rubik's cube as 54 stickers, each with a cubie position in {-1, 0, 1}^3
// and the direction it faces. A move rotates every sticker in one layer by
// 90 degrees about an axis -- no per-face permutation tables, and the same 3D
// coordinates map stickers onto the LED cube's faces. Hardware independent
// (test/test_rubiks).

#include <stdint.h>

namespace rubiks
{
    // Not WHITE, RED...: those are colour macros in cube_utils.h.
    enum Color : uint8_t
    {
        White,
        Yellow,
        Red,
        Orange,
        Green,
        Blue,
    };

    struct Sticker
    {
        int8_t pos[3];
        int8_t normal[3];
        Color color;
    };

    struct Move
    {
        uint8_t axis; // 0 x, 1 y, 2 z
        int8_t layer; // -1, 0 or 1
        int8_t dir;   // +1 or -1 quarter turn
        Move inverse() const { return Move{axis, layer, int8_t(-dir)}; }
    };

    class Cube
    {
    public:
        Cube() { reset(); }

        /// Solved: +z white, -z yellow, +x red, -x orange, +y green, -y blue.
        void reset();
        void apply(const Move &move);
        bool solved() const;

        /// The colour of the sticker at `pos` facing `normal`.
        Color at(const int8_t pos[3], const int8_t normal[3]) const;

        const Sticker *stickers() const { return _stickers; }

    private:
        Sticker _stickers[54];
    };

    /// A random outer-layer move that does not undo or repeat the last one
    /// on the same axis. `rnd` is any source of random integers.
    Move randomMove(const Move *previous, uint32_t (*rnd)());
}

#endif
