#ifndef NOISE_H
#define NOISE_H

// Ken Perlin's improved noise (2002) in 3D, for patterns that sample a field
// at each pixel's position on the cube (cube::toCube) so it flows around the
// edges. Hardware independent; tested on the host (test/test_noise).
//
// This is the scalar reference. A fixed-point or SIMD version, if one is ever
// written, must match it within a tolerance checked in the same tests.

namespace noise
{
    /// Smooth noise in roughly [-1, 1]; 0 at every integer lattice point.
    float perlin(float x, float y, float z);

    /// Sum of `octaves` layers of perlin(), each at twice the frequency and
    /// half the amplitude of the last, normalized back to roughly [-1, 1].
    float fbm(float x, float y, float z, int octaves);
}

#endif
