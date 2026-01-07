// =====================================================
//  Simple Tile-Based A* Pathfinder (DS-friendly)
// =====================================================

#include "pathfinder.h"

// -----------------------------------------------------
// INTERNAL HELPERS
// -----------------------------------------------------
int pf_abs(int v)
{
    return (v < 0) ? -v : v;
}

int pf_heuristic(int x1, int y1, int x2, int y2)
{
    return (pf_abs(x1 - x2) + pf_abs(y1 - y2)) * 10;
}

bool pf_walkable(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= WORLD_WIDTH || ty >= WORLD_HEIGHT)
        return false;

    return !checkCollision(TILE_TO_PX(tx), TILE_TO_PX(ty));
}

// -----------------------------------------------------
// PUBLIC API
// -----------------------------------------------------
/*
    pf_find_path()

    sx, sy : start tile
    gx, gy : goal tile
    out    : array of node indices (path output)
    maxlen : capacity of out[]

    returns number of steps (0 = no path)
*/
int pf_find_path(int sx, int sy, int gx, int gy, int *out, int maxlen)
{
    // Reset nodes
    for (int i = 0; i < MAX_NODES; i++)
    {
        pf_nodes[i].open = 0;
        pf_nodes[i].closed = 0;
        pf_nodes[i].parent = -1;
    }

    int start = NODE_INDEX(sx, sy);
    int goal = NODE_INDEX(gx, gy);

    pf_nodes[start].x = sx;
    pf_nodes[start].y = sy;
    pf_nodes[start].g = 0;
    pf_nodes[start].h = pf_heuristic(sx, sy, gx, gy);
    pf_nodes[start].open = 1;

    while (1)
    {
        int current = -1;
        int best_f = INT_MAX;

        // Find best open node
        for (int i = 0; i < MAX_NODES; i++)
        {
            if (pf_nodes[i].open)
            {
                int f = pf_nodes[i].g + pf_nodes[i].h;
                if (f < best_f)
                {
                    best_f = f;
                    current = i;
                }
            }
        }

        if (current == -1)
            return 0; // no path

        if (current == goal)
            break;

        pf_nodes[current].open = 0;
        pf_nodes[current].closed = 1;

        int cx = pf_nodes[current].x;
        int cy = pf_nodes[current].y;

        static const signed char dir[4][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

        for (int d = 0; d < 4; d++)
        {
            int nx = cx + dir[d][0];
            int ny = cy + dir[d][1];

            if (!pf_walkable(nx, ny))
                continue;

            int ni = NODE_INDEX(nx, ny);
            if (pf_nodes[ni].closed)
                continue;

            int new_g = pf_nodes[current].g + 10;

            if (!pf_nodes[ni].open || new_g < pf_nodes[ni].g)
            {
                pf_nodes[ni].x = nx;
                pf_nodes[ni].y = ny;
                pf_nodes[ni].g = new_g;
                pf_nodes[ni].h = pf_heuristic(nx, ny, gx, gy);
                pf_nodes[ni].parent = current;
                pf_nodes[ni].open = 1;
            }
        }
    }

    // Reconstruct path
    int len = 0;
    int cur = goal;

    while (cur != start && len < maxlen)
    {
        out[len++] = cur;
        cur = pf_nodes[cur].parent;
    }

    // Reverse
    for (int i = 0; i < len / 2; i++)
    {
        int t = out[i];
        out[i] = out[len - 1 - i];
        out[len - 1 - i] = t;
    }

    return len;
}

// -----------------------------------------------------
// OPTIONAL HELPERS
// -----------------------------------------------------
int pf_node_x(int node)
{
    return pf_nodes[node].x;
}

int pf_node_y(int node)
{
    return pf_nodes[node].y;
}

int find_closest_object(int sx, int sy, int object, bool (*is_target)(int tx, int ty, int object), int *out_tx, int *out_ty)
{
    static unsigned char visited[WORLD_WIDTH][WORLD_HEIGHT];
    static Tile queue[WORLD_WIDTH * WORLD_HEIGHT];

    // reset visited
    for (int y = 0; y < WORLD_HEIGHT; y++)
        for (int x = 0; x < WORLD_WIDTH; x++)
            visited[x][y] = 0;

    int qh = 0, qt = 0;

    queue[qt++] = (Tile){sx, sy};
    visited[sx][sy] = 1;

    while (qh < qt)
    {
        Tile t = queue[qh++];

        if (is_target(t.x, t.y, object))
        {
            *out_tx = t.x;
            *out_ty = t.y;
            return 1; // FOUND
        }

        static const signed char dir[4][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

        for (int i = 0; i < 4; i++)
        {
            int nx = t.x + dir[i][0];
            int ny = t.y + dir[i][1];

            if (nx < 0 || ny < 0 ||
                nx >= WORLD_WIDTH || ny >= WORLD_HEIGHT)
                continue;

            if (visited[nx][ny])
                continue;

            if (checkCollision(TILE_TO_PX(nx), TILE_TO_PX(ny)))
                continue;

            visited[nx][ny] = 1;
            queue[qt++] = (Tile){nx, ny};
        }
    }

    return 0; // NONE FOUND
}