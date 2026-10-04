#include "particles.h"

namespace
{
    const float S = cube::FACE_SIZE;

    float &axis(cube::Vec3 &v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
}

int particles::faceAxis(const cube::Vec3 &p)
{
    // The coordinate nearest 64 is the face's own; ties go to the top.
    if (p.z >= p.x && p.z >= p.y)
    {
        return 2;
    }
    return p.x >= p.y ? 0 : 1;
}

bool particles::move(cube::Vec3 &p, cube::Vec3 &v, const cube::Vec3 &a, float dt)
{
    int f = faceAxis(p);
    // Keep the particle exactly on its face, moving along it.
    axis(p, f) = S;
    cube::Vec3 acc = a;
    axis(acc, f) = 0;
    axis(v, f) = 0;
    v.x += acc.x * dt;
    v.y += acc.y * dt;
    v.z += acc.z * dt;
    p.x += v.x * dt;
    p.y += v.y * dt;
    p.z += v.z * dt;

    // Folding can only be needed over the face's two edges with the other
    // visible faces; a second fold in one step needs a huge step.
    for (int folds = 0; folds < 2; folds++)
    {
        int over = -1;
        for (int i = 0; i < 3; i++)
        {
            if (i != f && axis(p, i) > S)
            {
                over = i;
            }
        }
        if (over < 0)
        {
            break;
        }
        // Past the edge by `d` along axis `over`: on the new face (axis
        // `over` held at 64) that distance runs back down the old axis, and
        // the velocity across the edge turns the same way.
        const float d = axis(p, over) - S;
        axis(p, over) = S;
        axis(p, f) = S - d;
        const float speed = axis(v, over);
        axis(v, over) = 0;
        axis(v, f) = -speed;
        f = over;
    }
    return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x <= S && p.y <= S && p.z <= S;
}
