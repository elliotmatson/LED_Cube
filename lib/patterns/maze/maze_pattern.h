#ifndef MAZE_PATTERN_H
#define MAZE_PATTERN_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "maze.h"

/**
 * A maze across all three faces. Cells are 4x4-pixel blocks
 * (cube::buildBlockNeighbours), so corridors run over the seams. The maze
 * grows a few cells a frame (lib/maze, depth-first), the route from its
 * start to the farthest cell is drawn in, it holds with the route pulsing,
 * then fades and a new one grows in a new colour.
 */
class MazePattern : public Pattern
{
public:
    MazePattern();
    ~MazePattern();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int16_t BLOCK = 4;

private:
    enum class Phase : uint8_t
    {
        GROW,
        SOLVE,
        HOLD,
        FADE,
    };
    void restart();
    void drawCell(int cell, uint8_t mask, color::RGB c);
    void drawOpening(int cell, int dir, color::RGB c);
    void drawRoute(int upTo, uint8_t level);
    color::RGB routeColor(int i) const;

    int cells = 0;
    int16_t *nbr = nullptr;
    uint8_t *open = nullptr;
    uint8_t *revealed = nullptr;
    maze::Carve *events = nullptr;
    int16_t *scratchA = nullptr, *scratchB = nullptr, *scratchC = nullptr, *path = nullptr;
    uint8_t *visited = nullptr;
    int eventCount = 0, shown = 0, pathLength = 0, pathShown = 0;
    int head = -1;
    Phase phase = Phase::GROW;
    uint32_t phaseStartMs = 0;
    uint32_t seed = 1;
    uint8_t hue = 0;
};

#endif
