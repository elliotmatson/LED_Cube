#ifndef ALL_PATTERNS_H
#define ALL_PATTERNS_H

#include "cube_utils.h"

// Convenience include file. All patterns get included here
#include "snakes/snakes.h"
#include "plasma/plasma.h"
#include "spotify/spotify.h"
#include "clock/clock.h"
#include "game_of_life/game_of_life.h"
#include "ripples/ripples.h"
#include "matrix_rain/matrix_rain.h"
#include "nebula/nebula.h"
#include "wireframes/wireframes.h"
#include "rubiks_cube/rubiks_cube.h"
#include "falling_sand/falling_sand.h"
#include "plane_sweep/plane_sweep.h"
#include "ticker/ticker.h"
#include "dvd/dvd.h"
#include "pong/pong_pattern.h"


// Array of all patterns. The count is checked against the list in
// all_patterns.cpp at compile time.
#define PATTERN_COUNT 15
extern Pattern *patternList[PATTERN_COUNT];

#endif