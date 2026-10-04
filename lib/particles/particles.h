#ifndef PARTICLES_H
#define PARTICLES_H

// Points moving over the cube's visible surface, folding over the seams
// between its three faces. Hardware independent (test/test_particles).

#include "cube_geometry.h"

namespace particles
{
    /**
     * Moves `p` by `v * dt` over the visible surface: the three faces x = 64,
     * y = 64 and z = 64, each spanning [0, 64] in its other two coordinates.
     * Running off one face onto another folds over the shared edge: the
     * distance past it continues down the other face, and the velocity turns
     * with it. Running off an outer edge (a coordinate below 0) leaves the
     * surface.
     *
     * Acceleration `a` is applied first, keeping only its part along the
     * current face (gravity on a side face pulls down; on the top it does
     * nothing).
     *
     * @return false once the point has left the surface.
     */
    bool move(cube::Vec3 &p, cube::Vec3 &v, const cube::Vec3 &a, float dt);

    /// Which face `p` is on, as the axis held at 64: 0 = x, 1 = y, 2 = z.
    int faceAxis(const cube::Vec3 &p);
}

#endif
