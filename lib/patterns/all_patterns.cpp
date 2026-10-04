#include "all_patterns.h"

// Array of all patterns
// Order is the dashboard's button order. Saved selections are by id, so
// reordering or adding patterns is safe.
Pattern *patternList[] = {
    new SnakeGame(),
    new Plasma(),
    new Spotify(),
    new Clock(),
    new GameOfLife(),
    new Ripples(),
    new MatrixRain(),
    new Nebula(),
    new Wireframes(),
    new RubiksCube(),
    new FallingSand(),
    new PlaneSweep(),
    new Ticker(),
    new Dvd(),
    new Fire(),
    new Fireworks(),
    new LavaLamp(),
    new Aurora(),
    new Hyperspace(),
};