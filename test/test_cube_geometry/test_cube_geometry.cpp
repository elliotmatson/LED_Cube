#include <unity.h>
#include <math.h>

#include <cube_geometry.h>

using namespace cube;

// Everything a pattern draws goes through these mappings, and a mistake in one
// shows up as a seam: a snake turning back at an edge, a plasma field that
// does not line up, text spilling onto the next panel. These tests pin down
// the geometry so the patterns can rely on it.

void setUp(void) {}
void tearDown(void) {}

static float distance(Vec3 a, Vec3 b)
{
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// ---- faceToChain --------------------------------------------------------

void test_unrotated_face_maps_to_its_slice_of_the_chain(void)
{
    for (int f = 0; f < FACES; f++)
    {
        Point p = faceToChain(f, 0, 5, 7);
        TEST_ASSERT_EQUAL_INT16(f * FACE_SIZE + 5, p.x);
        TEST_ASSERT_EQUAL_INT16(7, p.y);
    }
}

void test_rotations_match_single_panel(void)
{
    // The mappings SinglePanel::getCoords used before it moved here.
    Point r1 = faceToChain(1, 1, 10, 20);
    TEST_ASSERT_EQUAL_INT16(64 + 20, r1.x);
    TEST_ASSERT_EQUAL_INT16(63 - 10, r1.y);

    Point r2 = faceToChain(2, 2, 10, 20);
    TEST_ASSERT_EQUAL_INT16(128 + 63 - 10, r2.x);
    TEST_ASSERT_EQUAL_INT16(63 - 20, r2.y);

    Point r3 = faceToChain(0, 3, 10, 20);
    TEST_ASSERT_EQUAL_INT16(63 - 20, r3.x);
    TEST_ASSERT_EQUAL_INT16(10, r3.y);
}

void test_every_rotation_is_a_bijection_onto_its_face(void)
{
    for (int rotation = 0; rotation < 4; rotation++)
    {
        static bool seen[FACE_SIZE][FACE_SIZE];
        for (int i = 0; i < FACE_SIZE; i++)
            for (int j = 0; j < FACE_SIZE; j++)
                seen[i][j] = false;

        for (int16_t x = 0; x < FACE_SIZE; x++)
        {
            for (int16_t y = 0; y < FACE_SIZE; y++)
            {
                Point p = faceToChain(1, rotation, x, y);
                TEST_ASSERT_TRUE(p.valid());
                TEST_ASSERT_EQUAL_INT(1, face(p.x));
                TEST_ASSERT_FALSE_MESSAGE(seen[p.x - FACE_SIZE][p.y], "two pixels mapped to one");
                seen[p.x - FACE_SIZE][p.y] = true;
            }
        }
    }
}

// Unclipped drawing used to bleed onto the neighbouring panel.
void test_pixels_off_the_face_are_rejected_not_wrapped(void)
{
    TEST_ASSERT_FALSE(faceToChain(0, 0, 64, 0).valid());
    TEST_ASSERT_FALSE(faceToChain(0, 0, -1, 0).valid());
    TEST_ASSERT_FALSE(faceToChain(1, 2, 0, 64).valid());
    TEST_ASSERT_FALSE(faceToChain(2, 1, 0, -1).valid());
    TEST_ASSERT_FALSE(faceToChain(3, 0, 0, 0).valid());
}

// ---- step ---------------------------------------------------------------

void test_steps_within_a_face_move_one_pixel(void)
{
    Step s = step({10, 10}, RIGHT);
    TEST_ASSERT_EQUAL_INT16(11, s.to.x);
    TEST_ASSERT_EQUAL_INT16(10, s.to.y);
    TEST_ASSERT_EQUAL(RIGHT, s.dir);

    s = step({70, 10}, UP);
    TEST_ASSERT_EQUAL_INT16(70, s.to.x);
    TEST_ASSERT_EQUAL_INT16(9, s.to.y);
    TEST_ASSERT_EQUAL(UP, s.dir);
}

void test_outer_edges_are_walls(void)
{
    TEST_ASSERT_FALSE(step({10, 0}, UP).valid());   // face 0 top
    TEST_ASSERT_FALSE(step({0, 10}, LEFT).valid());  // face 0 left
    TEST_ASSERT_FALSE(step({64, 10}, LEFT).valid()); // face 1 outer edge
    TEST_ASSERT_FALSE(step({191, 10}, RIGHT).valid()); // face 2 outer edge
    TEST_ASSERT_FALSE(step({100, 0}, UP).valid());  // face 1 top
    TEST_ASSERT_FALSE(step({150, 0}, UP).valid());  // face 2 top
}

void test_seams_land_where_the_snake_pattern_expects(void)
{
    // The adjacency Snake's check_move used before it moved here.
    Step s = step({63, 20}, RIGHT);
    TEST_ASSERT_EQUAL_INT16(64 + 20, s.to.x);
    TEST_ASSERT_EQUAL_INT16(63, s.to.y);

    s = step({20, 63}, DOWN);
    TEST_ASSERT_EQUAL_INT16(191 - 20, s.to.x);
    TEST_ASSERT_EQUAL_INT16(63, s.to.y);

    s = step({84, 63}, DOWN);
    TEST_ASSERT_EQUAL_INT16(63, s.to.x);
    TEST_ASSERT_EQUAL_INT16(20, s.to.y);

    s = step({127, 30}, RIGHT);
    TEST_ASSERT_EQUAL_INT16(128, s.to.x);
    TEST_ASSERT_EQUAL_INT16(30, s.to.y);
}

// The old check_move kept the direction unchanged across a seam. Arriving on
// face 2's bottom row still heading DOWN means the next step goes straight
// back to face 0, so snakes bounced at every seam instead of crossing it.
void test_crossing_a_seam_keeps_heading_away_from_it(void)
{
    for (int16_t i = 0; i < FACE_SIZE; i++)
    {
        const Point starts[] = {{63, i}, {i, 63}, {int16_t(64 + i), 63}, {int16_t(128 + i), 63}, {127, i}, {128, i}};
        const Dir dirs[] = {RIGHT, DOWN, DOWN, DOWN, RIGHT, LEFT};
        for (int k = 0; k < 6; k++)
        {
            Step first = step(starts[k], dirs[k]);
            TEST_ASSERT_TRUE(first.valid());
            Step second = step(first.to, first.dir);
            TEST_ASSERT_TRUE_MESSAGE(second.valid(), "walked off the cube right after a seam");
            TEST_ASSERT_FALSE_MESSAGE(second.to.x == starts[k].x && second.to.y == starts[k].y,
                                      "second step went back across the seam");
        }
    }
}

// Every step can be undone by stepping back the opposite way. This is what
// makes the seams consistent from both sides.
void test_every_step_is_reversible(void)
{
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            for (uint8_t d = 0; d < 4; d++)
            {
                Step there = step({x, y}, Dir(d));
                if (!there.valid())
                {
                    continue;
                }
                Step back = step(there.to, opposite(there.dir));
                TEST_ASSERT_TRUE(back.valid());
                TEST_ASSERT_EQUAL_INT16(x, back.to.x);
                TEST_ASSERT_EQUAL_INT16(y, back.to.y);
                TEST_ASSERT_EQUAL(opposite(Dir(d)), back.dir);
            }
        }
    }
}

// ---- toCube -------------------------------------------------------------

void test_every_step_is_between_neighbouring_points_in_3d(void)
{
    // One pixel apart on a face, or two half-pixels around an edge.
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            for (uint8_t d = 0; d < 4; d++)
            {
                Step s = step({x, y}, Dir(d));
                if (!s.valid())
                {
                    continue;
                }
                float dist = distance(toCube({x, y}), toCube(s.to));
                bool sameFace = face(x) == face(s.to.x);
                TEST_ASSERT_FLOAT_WITHIN(0.001f, sameFace ? 1.0f : 0.70711f, dist);
            }
        }
    }
}

void test_every_pixel_lies_on_a_visible_face_of_the_cube(void)
{
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            Vec3 v = toCube({x, y});
            TEST_ASSERT_TRUE(v.x >= 0 && v.x <= 64 && v.y >= 0 && v.y <= 64 && v.z >= 0 && v.z <= 64);
            TEST_ASSERT_TRUE(v.x == 64 || v.y == 64 || v.z == 64);
        }
    }
}

// ---- neighbours ---------------------------------------------------------

static int16_t neighbours[CELLS * NEIGHBOURS];

static bool hasNeighbour(int cell, int other)
{
    for (int k = 0; k < NEIGHBOURS; k++)
    {
        if (neighbours[cell * NEIGHBOURS + k] == other)
            return true;
    }
    return false;
}

void test_interior_cells_have_the_usual_eight_neighbours(void)
{
    buildNeighbours(neighbours);
    const int cell = cellIndex({100, 30});
    const int expected[] = {cellIndex({99, 29}), cellIndex({100, 29}), cellIndex({101, 29}),
                            cellIndex({99, 30}), cellIndex({101, 30}),
                            cellIndex({99, 31}), cellIndex({100, 31}), cellIndex({101, 31})};
    for (int e : expected)
    {
        TEST_ASSERT_TRUE(hasNeighbour(cell, e));
    }
}

// Game of Life across the seams is only fair if "b is next to a" always
// means "a is next to b".
void test_neighbours_are_symmetric(void)
{
    buildNeighbours(neighbours);
    for (int cell = 0; cell < CELLS; cell++)
    {
        for (int k = 0; k < NEIGHBOURS; k++)
        {
            int other = neighbours[cell * NEIGHBOURS + k];
            if (other >= 0)
            {
                TEST_ASSERT_TRUE_MESSAGE(hasNeighbour(other, cell), "one-way neighbour");
            }
        }
    }
}

void test_neighbours_are_close_on_the_cube(void)
{
    buildNeighbours(neighbours);
    for (int cell = 0; cell < CELLS; cell++)
    {
        Point p{int16_t(cell % CHAIN_WIDTH), int16_t(cell / CHAIN_WIDTH)};
        for (int k = 0; k < NEIGHBOURS; k++)
        {
            int other = neighbours[cell * NEIGHBOURS + k];
            if (other >= 0)
            {
                Point q{int16_t(other % CHAIN_WIDTH), int16_t(other / CHAIN_WIDTH)};
                TEST_ASSERT_TRUE(distance(toCube(p), toCube(q)) < 1.5f);
            }
        }
    }
}

void test_seam_cells_have_a_full_set_of_neighbours(void)
{
    buildNeighbours(neighbours);
    // Middle of each seam, on both sides: away from the outer edges and the
    // shared corner, so all eight should exist.
    const Point seamCells[] = {{63, 30}, {94, 63}, {30, 63}, {161, 63}, {127, 30}, {128, 30}};
    for (Point p : seamCells)
    {
        int count = 0;
        for (int k = 0; k < NEIGHBOURS; k++)
        {
            count += neighbours[cellIndex(p) * NEIGHBOURS + k] >= 0;
        }
        TEST_ASSERT_EQUAL_INT(8, count);
    }
}

// ---- projection ---------------------------------------------------------

// Plasma looks continuous across the seams only because neighbouring pixels
// project close together. Within a face that is one unit. Across a seam the
// projection leaves a gap of about three -- 3.0 between faces 0 and 1, 3.5 on
// the other two seams, measured as distance in the projected plane -- which
// stands in for the panel bezels; the patterns were tuned with it, so it is
// pinned here rather than "fixed".
void test_projection_is_continuous_across_seams(void)
{
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            for (uint8_t d = 0; d < 4; d++)
            {
                Step s = step({x, y}, Dir(d));
                if (!s.valid())
                {
                    continue;
                }
                const bool seam = face(x) != face(s.to.x);
                const int tolerance = seam ? 4 : 1;
                TEST_ASSERT_INT_WITHIN(tolerance, projectX(x, y), projectX(s.to.x, s.to.y));
                TEST_ASSERT_INT_WITHIN(tolerance, projectY(x, y), projectY(s.to.x, s.to.y));
                TEST_ASSERT_FLOAT_WITHIN(seam ? 3.5f : 0.9f, projectXf(x, y), projectXf(s.to.x, s.to.y));
            }
        }
    }
}

void test_integer_projection_tracks_the_float_one(void)
{
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            TEST_ASSERT_FLOAT_WITHIN(8.0f, projectXf(x, y), projectX(x, y));
        }
    }
}

void test_unproject_inverts_the_projection_for_every_pixel(void)
{
    for (int16_t x = 0; x < CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            Point p = unproject(projectXf(x, y), projectYf(x, y));
            TEST_ASSERT_EQUAL_INT16(x, p.x);
            TEST_ASSERT_EQUAL_INT16(y, p.y);
        }
    }
}

void test_unproject_rejects_points_outside_the_hexagon(void)
{
    TEST_ASSERT_FALSE(unproject(200, 0).valid());
    TEST_ASSERT_FALSE(unproject(0, 200).valid());
    TEST_ASSERT_FALSE(unproject(0, -200).valid());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_unrotated_face_maps_to_its_slice_of_the_chain);
    RUN_TEST(test_rotations_match_single_panel);
    RUN_TEST(test_every_rotation_is_a_bijection_onto_its_face);
    RUN_TEST(test_pixels_off_the_face_are_rejected_not_wrapped);
    RUN_TEST(test_steps_within_a_face_move_one_pixel);
    RUN_TEST(test_outer_edges_are_walls);
    RUN_TEST(test_seams_land_where_the_snake_pattern_expects);
    RUN_TEST(test_crossing_a_seam_keeps_heading_away_from_it);
    RUN_TEST(test_every_step_is_reversible);
    RUN_TEST(test_every_step_is_between_neighbouring_points_in_3d);
    RUN_TEST(test_every_pixel_lies_on_a_visible_face_of_the_cube);
    RUN_TEST(test_interior_cells_have_the_usual_eight_neighbours);
    RUN_TEST(test_neighbours_are_symmetric);
    RUN_TEST(test_neighbours_are_close_on_the_cube);
    RUN_TEST(test_seam_cells_have_a_full_set_of_neighbours);
    RUN_TEST(test_projection_is_continuous_across_seams);
    RUN_TEST(test_integer_projection_tracks_the_float_one);
    RUN_TEST(test_unproject_inverts_the_projection_for_every_pixel);
    RUN_TEST(test_unproject_rejects_points_outside_the_hexagon);
    return UNITY_END();
}
