#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include "defs.h"

// -----------------------------------------------------
// CONFIG
// -----------------------------------------------------
#define MAX_NODES (WORLD_WIDTH * WORLD_HEIGHT)

// -----------------------------------------------------
// MACROS
// -----------------------------------------------------
#define PX_TO_TILE(x) ((x) >> 4)
#define TILE_TO_PX(x) ((x) << 4)
#define NODE_INDEX(x, y) ((y) * WORLD_WIDTH + (x))

// -----------------------------------------------------
// NODE STRUCT
// -----------------------------------------------------
typedef struct
{
    short x, y;
    short g, h;
    short parent;
    unsigned char open;
    unsigned char closed;
} PF_Node;

typedef struct {
    short x, y;
} Tile;

// -----------------------------------------------------
// INTERNAL STATE
// -----------------------------------------------------
static PF_Node pf_nodes[MAX_NODES];

bool checkCollision(int newX, int newY);
int pf_abs(int v);
int pf_heuristic(int x1, int y1, int x2, int y2);
bool pf_walkable(int tx, int ty);
int pf_find_path(int sx, int sy, int gx, int gy, int *out, int maxlen);
int pf_node_x(int node);
int pf_node_y(int node);
int find_closest_object(int sx, int sy, int object, bool (*is_target)(int tx, int ty, int object), int *out_tx, int *out_ty);