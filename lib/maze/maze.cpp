#include "maze.h"

namespace
{
    uint32_t next(uint32_t &s)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }

    /// The direction from `to` back to `from`, or -1.
    int back(const int16_t *nbr, int from, int to)
    {
        for (int d = 0; d < 4; d++)
        {
            if (nbr[to * 4 + d] == from)
            {
                return d;
            }
        }
        return -1;
    }
}

int maze::generate(const int16_t *nbr, int cells, int start, uint8_t *open, Carve *events, int16_t *stack,
                   uint8_t *visited, uint32_t &seed)
{
    for (int i = 0; i < cells; i++)
    {
        open[i] = 0;
        visited[i] = 0;
    }
    int top = 0, count = 0;
    stack[top++] = int16_t(start);
    visited[start] = 1;
    while (top > 0)
    {
        const int cell = stack[top - 1];
        // Pick an unvisited neighbour at random; backtrack if there is none.
        int choices[4], n = 0;
        for (int d = 0; d < 4; d++)
        {
            const int to = nbr[cell * 4 + d];
            if (to >= 0 && !visited[to])
            {
                choices[n++] = d;
            }
        }
        if (n == 0)
        {
            top--;
            continue;
        }
        const int d = choices[next(seed) % n];
        const int to = nbr[cell * 4 + d];
        open[cell] |= uint8_t(1 << d);
        const int b = back(nbr, cell, to);
        if (b >= 0)
        {
            open[to] |= uint8_t(1 << b);
        }
        visited[to] = 1;
        events[count++] = {int16_t(cell), uint8_t(d)};
        stack[top++] = int16_t(to);
    }
    return count;
}

int maze::search(const int16_t *nbr, const uint8_t *open, int cells, int start, int16_t *dist, int16_t *prev,
                 int16_t *queue)
{
    for (int i = 0; i < cells; i++)
    {
        dist[i] = -1;
        prev[i] = -1;
    }
    int head = 0, tail = 0, farthest = start;
    dist[start] = 0;
    queue[tail++] = int16_t(start);
    while (head < tail)
    {
        const int cell = queue[head++];
        if (dist[cell] > dist[farthest])
        {
            farthest = cell;
        }
        for (int d = 0; d < 4; d++)
        {
            const int to = nbr[cell * 4 + d];
            if ((open[cell] & (1 << d)) && to >= 0 && dist[to] < 0)
            {
                dist[to] = int16_t(dist[cell] + 1);
                prev[to] = int16_t(cell);
                queue[tail++] = int16_t(to);
            }
        }
    }
    return farthest;
}

int maze::route(const int16_t *prev, int goal, int16_t *out, int capacity)
{
    int n = 0;
    for (int c = goal; c >= 0 && n < capacity; c = prev[c])
    {
        out[n++] = int16_t(c);
    }
    // Collected goal first: reverse.
    for (int i = 0; i < n / 2; i++)
    {
        const int16_t t = out[i];
        out[i] = out[n - 1 - i];
        out[n - 1 - i] = t;
    }
    return n;
}
