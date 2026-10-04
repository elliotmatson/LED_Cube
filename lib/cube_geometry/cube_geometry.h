#ifndef CUBE_GEOMETRY_H
#define CUBE_GEOMETRY_H

// Geometry of the cube's display surface: three 64x64 faces sharing one
// corner, driven as one 192x64 HUB75 chain. Face f occupies chain columns
// [64f, 64f+63].
//
// No Arduino or ESP-IDF includes: this is compiled into the host unit tests
// (test/test_cube_geometry) as well as the firmware.

#include <stdint.h>

namespace cube
{
    constexpr int16_t FACE_SIZE = 64;
    constexpr int16_t FACES = 3;
    constexpr int16_t CHAIN_WIDTH = FACE_SIZE * FACES;
    constexpr int16_t CHAIN_HEIGHT = FACE_SIZE;

    /// A pixel on the chain. x < 0 means "no such pixel".
    struct Point
    {
        int16_t x;
        int16_t y;
        bool valid() const { return x >= 0; }
    };

    constexpr Point NO_POINT{-1, -1};

    /// Directions in a face's own chain coordinates: Up is decreasing y.
    enum Dir : uint8_t
    {
        UP = 0,
        RIGHT = 1,
        DOWN = 2,
        LEFT = 3,
    };

    inline Dir opposite(Dir d) { return static_cast<Dir>((d + 2) & 3); }

    /// The face a chain column belongs to.
    inline int face(int16_t x) { return x / FACE_SIZE; }

    /**
     * Maps a pixel of a rotated 64x64 view of one face onto the chain.
     *
     * @param faceIndex Physical face, 0-2.
     * @param rotation Quarter turns, 0-3 (see SinglePanel::setRotation).
     * @return NO_POINT when (x, y) is outside the face, so drawing clips at
     * the face edge instead of spilling onto the neighbouring panel.
     */
    Point faceToChain(int faceIndex, int rotation, int16_t x, int16_t y);

    /// Result of moving one pixel across the cube surface.
    struct Step
    {
        Point to;
        /// The direction of travel on arrival, in the new face's coordinates.
        /// Differs from the starting direction when the step crosses a seam
        /// between faces whose coordinate systems are rotated relative to
        /// each other.
        Dir dir;
        bool valid() const { return to.valid(); }
    };

    /**
     * Moves one pixel from `from` in direction `dir`, crossing seams between
     * faces. Returns an invalid step at the cube's outer edges.
     *
     * The seams, in chain coordinates:
     * - face 0 right edge (x = 63)  <-> face 1 bottom row (y = 63), x' = 64 + y
     * - face 0 bottom row (y = 63)  <-> face 2 bottom row (y = 63), x' = 191 - x
     * - face 1 right edge (x = 127) <-> face 2 left edge (x = 128), same y
     */
    Step step(Point from, Dir dir);

    constexpr int NEIGHBOURS = 8;
    constexpr int CELLS = CHAIN_WIDTH * CHAIN_HEIGHT;

    /// Index of a chain pixel in a row-major 192x64 array.
    inline int cellIndex(Point p) { return p.y * CHAIN_WIDTH + p.x; }

    /**
     * The eight neighbours of every pixel on the cube surface, for cellular
     * automata that run across the seams: out[cell * 8 + k] is a cell index,
     * or -1 past the cube's outer edge.
     *
     * The four orthogonal neighbours come from step(); each diagonal is a
     * step followed by a right turn in the heading step() arrived with, so
     * diagonals stay consistent across seams. At the shared corner, where
     * three faces meet, a cell has fewer than eight distinct neighbours;
     * duplicates are reported as -1.
     *
     * @param out CELLS * NEIGHBOURS entries.
     */
    void buildNeighbours(int16_t *out);

    /**
     * Neighbours of square blocks of `size` pixels (size divides FACE_SIZE),
     * across the seams: block (bx, by) covers chain pixels from (bx * size,
     * by * size) and has index by * (CHAIN_WIDTH / size) + bx.
     * out[block * 4 + d] is the block reached by leaving it in direction d
     * (in its face's coordinates), or -1 past an outer edge. Because 64 is a
     * multiple of `size`, blocks line up across the seams.
     *
     * @param out blockCount(size) * 4 entries.
     */
    void buildBlockNeighbours(int16_t size, int16_t *out);

    /// How many blocks of `size` pixels tile the chain.
    inline int blockCount(int16_t size) { return (CHAIN_WIDTH / size) * (CHAIN_HEIGHT / size); }

    /// A point in the cube's own 3D space, [0, 64] on each axis.
    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    /**
     * The centre of a chain pixel as a point on the surface of the cube, with
     * the shared visible corner at (64, 64, 64). Face 0 is the z = 64 plane,
     * face 1 x = 64, face 2 y = 64. Pixels that are neighbours across a seam
     * (see step()) are neighbours here too, which is what lets volumetric
     * patterns sample a 3D field and look continuous across the edges.
     */
    Vec3 toCube(Point p);

    /**
     * The inverse of toCube(): the chain pixel showing point `v` of the
     * cube's visible surface (one of x, y, z at 64, the others in [0, 64]),
     * or NO_POINT if `v` is not on it. On an edge shared by two faces the
     * top face wins, then face 1.
     */
    Point fromCube(Vec3 v);

    /// Isometric projection of a chain pixel onto the plane facing the shared
    /// corner, continuous across all three seams. X is roughly ±56; Y runs
    /// from about -64 (far corners of faces 1 and 2) to 65 (far corner of
    /// face 0). Integer and floating-point versions; the integer one is a
    /// faster approximation used by Plasma.
    int16_t projectX(int16_t x, int16_t y);
    float projectXf(int16_t x, int16_t y);
    int16_t projectY(int16_t x, int16_t y);
    float projectYf(int16_t x, int16_t y);

    /**
     * The inverse of (projectXf, projectYf): the pixel that shows point
     * (X, Y) of the projected plane, or NO_POINT if no pixel does (outside
     * the hexagon, or in the gaps the projection leaves along the seams).
     * Lets a pattern draw in the projected plane -- a wireframe floating in
     * the cube, say -- and land on the right face.
     */
    Point unproject(float X, float Y);
}

#endif
