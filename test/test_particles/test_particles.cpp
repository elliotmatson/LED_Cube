#include <unity.h>

#include <particles.h>

static const cube::Vec3 NO_ACCEL = {0, 0, 0};

void setUp(void) {}
void tearDown(void) {}

void test_moves_along_a_face(void)
{
    cube::Vec3 p = {64, 10, 10}, v = {0, 2, 3};
    TEST_ASSERT_TRUE(particles::move(p, v, NO_ACCEL, 1));
    TEST_ASSERT_EQUAL_FLOAT(64, p.x);
    TEST_ASSERT_EQUAL_FLOAT(12, p.y);
    TEST_ASSERT_EQUAL_FLOAT(13, p.z);
}

void test_rising_off_a_side_folds_onto_the_top(void)
{
    // Face x = 64, rising 4 px past the top edge.
    cube::Vec3 p = {64, 20, 62}, v = {0, 0, 6};
    TEST_ASSERT_TRUE(particles::move(p, v, NO_ACCEL, 1));
    TEST_ASSERT_EQUAL_FLOAT(64, p.z);
    TEST_ASSERT_EQUAL_FLOAT(60, p.x); // 4 px in from the edge
    TEST_ASSERT_EQUAL_FLOAT(20, p.y);
    TEST_ASSERT_EQUAL_FLOAT(-6, v.x); // now heading away from the edge
    TEST_ASSERT_EQUAL_FLOAT(0, v.z);
    TEST_ASSERT_EQUAL_INT(2, particles::faceAxis(p));
}

void test_sliding_off_the_top_folds_down_a_side(void)
{
    cube::Vec3 p = {30, 62, 64}, v = {0, 5, 0};
    TEST_ASSERT_TRUE(particles::move(p, v, NO_ACCEL, 1));
    TEST_ASSERT_EQUAL_FLOAT(64, p.y);
    TEST_ASSERT_EQUAL_FLOAT(61, p.z); // 3 px down the side
    TEST_ASSERT_EQUAL_FLOAT(-5, v.z);
}

void test_crossing_between_the_side_faces(void)
{
    cube::Vec3 p = {64, 63, 30}, v = {0, 3, 0};
    TEST_ASSERT_TRUE(particles::move(p, v, NO_ACCEL, 1));
    TEST_ASSERT_EQUAL_FLOAT(64, p.y);
    TEST_ASSERT_EQUAL_FLOAT(62, p.x);
    TEST_ASSERT_EQUAL_FLOAT(-3, v.x);
}

void test_leaving_by_an_outer_edge_ends_it(void)
{
    cube::Vec3 p = {64, 1, 30}, v = {0, -5, 0};
    TEST_ASSERT_FALSE(particles::move(p, v, NO_ACCEL, 1));
    cube::Vec3 q = {64, 30, 2}, w = {0, 0, -5}; // off the bottom
    TEST_ASSERT_FALSE(particles::move(q, w, NO_ACCEL, 1));
}

void test_gravity_pulls_down_a_side_but_not_across_the_top(void)
{
    const cube::Vec3 g = {0, 0, -10};
    cube::Vec3 p = {64, 30, 40}, v = {0, 0, 0};
    particles::move(p, v, g, 0.5f);
    TEST_ASSERT_TRUE(v.z < 0);
    cube::Vec3 q = {30, 30, 64}, w = {1, 0, 0};
    particles::move(q, w, g, 0.5f);
    TEST_ASSERT_EQUAL_FLOAT(64, q.z);
    TEST_ASSERT_EQUAL_FLOAT(0, w.z);
    TEST_ASSERT_EQUAL_FLOAT(1, w.x);
}

void test_a_folded_particle_maps_to_a_top_face_pixel(void)
{
    cube::Vec3 p = {64, 20, 63}, v = {0, 0, 4};
    particles::move(p, v, NO_ACCEL, 1);
    const cube::Point px = cube::fromCube(p);
    TEST_ASSERT_TRUE(px.valid());
    TEST_ASSERT_EQUAL_INT(0, cube::face(px.x));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_moves_along_a_face);
    RUN_TEST(test_rising_off_a_side_folds_onto_the_top);
    RUN_TEST(test_sliding_off_the_top_folds_down_a_side);
    RUN_TEST(test_crossing_between_the_side_faces);
    RUN_TEST(test_leaving_by_an_outer_edge_ends_it);
    RUN_TEST(test_gravity_pulls_down_a_side_but_not_across_the_top);
    RUN_TEST(test_a_folded_particle_maps_to_a_top_face_pixel);
    return UNITY_END();
}
