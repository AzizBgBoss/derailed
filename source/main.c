#include <stdio.h>
#include <nds.h>
#include <time.h>

#include "perlin.h"

#include "tilemap.h"
#include "player.h"
#include "ui.h"
#include "wagons.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

#define WORLD_WIDTH 128
#define WORLD_HEIGHT 12
#define TILE_SIZE 16
#define PLAYER_SIZE 16

#define EMPTY 0
#define TILE_EMPTY 1
#define TILE_TREE 2
#define TILE_ROCK 3
#define OBJECT_RAIL 8
#define OBJECT_WOOD 9
#define OBJECT_IRON 10
#define OBJECT_AXE 11
#define OBJECT_PICKAXE 12

#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3

// TODO: My code is very not clean, i need to get rid of repeated expressions and split the code into functions
// There's also a lot of magic numbers, i need to define them

int bg0;
uint16_t *bg0Map;
int bg1;
uint16_t *bg1Map;

unsigned int frames = 0;

int scroll;
int chunk; // Current chunk loaded (each chunk is 4 tiles wide)

int lastPlacedX;
int lastPlacedY;
bool justPlaced;

struct Player
{
    float x;
    float y;
    int direction;
    int objectHeld;
    int quantityHeld;
    int maxQuantityHeld;
    int selectedObjectX;
    int selectedObjectY;
    bool selectedObject;
    int selectedWagonId;
    int selectedWagonSlot;
    bool selectedWagon;
};

struct Wagon
{
    u16 *gfx;
    float x;
    float y;
    int sizeX;
    int sizeY;
    int direction;
    float speed;
    int slots[2];
    int quantity[2];
    int maxQuantity;
    int acceptedObjects[2];
};

struct Player player = {WORLD_WIDTH * TILE_SIZE / 2, WORLD_HEIGHT *TILE_SIZE / 2, DIR_DOWN, 0, 0, 3, 0, 0, false};

struct Wagon locomotive = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.01f, {EMPTY, EMPTY}, {0, 0}, 0, {EMPTY, EMPTY}};
struct Wagon railStorage = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.0f, {EMPTY, EMPTY}, {0, 0}, 3, {OBJECT_IRON, OBJECT_WOOD}};
struct Wagon railBuilder = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.0f, {EMPTY, EMPTY}, {0, 0}, 3, {EMPTY, EMPTY}};

#define WAGONS 3

struct Wagon *wagons[WAGONS] = {&locomotive, &railStorage, &railBuilder};

uint8_t worldTerrain[WORLD_WIDTH][WORLD_HEIGHT];
uint8_t worldVariants[WORLD_WIDTH][WORLD_HEIGHT];
uint8_t worldObjects[WORLD_WIDTH][WORLD_HEIGHT];
uint8_t worldHealth[WORLD_WIDTH][WORLD_HEIGHT];

void bg0SetTile(int x, int y, int tile)
{
    if (x < 0 || x >= 64 || y < 0 || y >= 32)
        return;
    if (x < 32)
        bg0Map[x + y * 32] = tile;
    else
        bg0Map[x - 32 + (y + 32) * 32] = tile;
}
void bg1SetTile(int x, int y, int tile)
{
    if (x < 0 || x >= 64 || y < 0 || y >= 32)
        return;
    if (x < 32)
        bg1Map[x + y * 32] = tile;
    else
        bg1Map[x - 32 + (y + 32) * 32] = tile;
}

void setWorldTile(int x, int y, int tile)
{
    worldTerrain[x][y] = tile;
    worldHealth[x][y] = 3;
    worldVariants[x][y] = rand() % 4;

    if (x >= (chunk - 5) * 4 && x < chunk * 4)
    {
        bg0SetTile((x * 2) % 64, y * 2, tile * 4 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3 + worldVariants[x][y] * 16 * 4);
    }
}

void setWorldObject(int x, int y, int tile)
{
    worldObjects[x][y] = tile;

    if (x >= (chunk - 5) * 4 && x < chunk * 4)
    {
        bg1SetTile((x * 2) % 64, y * 2, tile * 4);
        bg1SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1);
        bg1SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2);
        bg1SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3);
    }
}

void setWorldHealth(int x, int y, int health)
{
    worldHealth[x][y] = health;
    if (worldHealth[x][y] == 0)
    {
        if (worldTerrain[x][y] == TILE_TREE)
            setWorldObject(x, y, OBJECT_WOOD);
        else if (worldTerrain[x][y] == TILE_ROCK)
            setWorldObject(x, y, OBJECT_IRON);
        setWorldTile(x, y, TILE_EMPTY);
        return;
    }
    int tile = worldTerrain[x][y] + (3 - health) * 2;
    if (x >= (chunk - 5) * 4 && x < chunk * 4)
    {
        bg0SetTile((x * 2) % 64, y * 2, tile * 4 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2 + worldVariants[x][y] * 16 * 4);
        bg0SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3 + worldVariants[x][y] * 16 * 4);
    }
}

static inline bool isSolidTerrain(int tx, int ty)
{
    if (tx < 0 || tx >= WORLD_WIDTH || ty < 0 || ty >= WORLD_HEIGHT)
        return true; // treat out of bounds as solid

    return (worldTerrain[tx][ty] != TILE_EMPTY);
}

bool checkCollision(int newX, int newY)
{
    int left = newX / TILE_SIZE;
    int right = (newX + PLAYER_SIZE - 1) / TILE_SIZE;
    int top = newY / TILE_SIZE;
    int bottom = (newY + PLAYER_SIZE - 1) / TILE_SIZE;

    if (newX < 0 || newX + PLAYER_SIZE > WORLD_WIDTH * TILE_SIZE || newY < 0 || newY + PLAYER_SIZE > WORLD_HEIGHT * TILE_SIZE)
        return true;

    for (int i = 0; i < WAGONS; i++)
    {
        struct Wagon *wagon = wagons[i];
        if ((newX >= wagon->x && newX < wagon->x + wagon->sizeX &&
             newY >= wagon->y && newY < wagon->y + wagon->sizeY) ||
            (newX + PLAYER_SIZE >= wagon->x && newX + PLAYER_SIZE < wagon->x + wagon->sizeX &&
             newY >= wagon->y && newY < wagon->y + wagon->sizeY) ||
            (newX >= wagon->x && newX < wagon->x + wagon->sizeX &&
             newY + PLAYER_SIZE >= wagon->y && newY + PLAYER_SIZE < wagon->y + wagon->sizeY) ||
            (newX + PLAYER_SIZE >= wagon->x && newX + PLAYER_SIZE < wagon->x + wagon->sizeX &&
             newY + PLAYER_SIZE >= wagon->y && newY + PLAYER_SIZE < wagon->y + wagon->sizeY))
            return true;
    }

    if (isSolidTerrain(left, top))
        return true;
    if (isSolidTerrain(right, top))
        return true;
    if (isSolidTerrain(left, bottom))
        return true;
    if (isSolidTerrain(right, bottom))
        return true;

    return false;
}

void updateWagon(int id)
{
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[0], wagons[id]->gfx, 8 * 8 * 2);                      // Top-left quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[1] + 8 * 4, wagons[id]->gfx + 8 * 8, 8 * 8 * 2);      // Top-right quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[0] + 8 * 8, wagons[id]->gfx + 8 * 8 * 2, 8 * 8 * 2);  // Bottom-left quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[1] + 8 * 12, wagons[id]->gfx + 8 * 8 * 3, 8 * 8 * 2); // Bottom-right quarter
}

void generateWorld(int seed)
{
    printf("Generating world with seed %d\n", seed);
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int y = 0; y < WORLD_HEIGHT; y++)
        {
            setWorldTile(x, y, TILE_EMPTY);
            setWorldObject(x, y, EMPTY);
        }
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int y = 0; y < WORLD_HEIGHT; y++)
        {
            float noiseValue = fractalPerlin2D(x * 0.1f, y * 0.1f, 4, 0.5f, 1.0f, seed);
            if (noiseValue < -0.2f)
            {
                setWorldTile(x, y, TILE_ROCK);
            }
            else if (noiseValue < 0.2f)
            {
                setWorldTile(x, y, TILE_EMPTY);
            }
            else
            {
                setWorldTile(x, y, TILE_TREE);
            }
        }
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        setWorldTile(x, 5, TILE_EMPTY); // Clear a path in the middle
        if (x < 8)
        {
            setWorldObject(x, 5, OBJECT_RAIL);
            setWorldTile(x, 6, TILE_EMPTY);
        }
    }
    setWorldObject(0, 6, OBJECT_WOOD);
    setWorldObject(1, 6, OBJECT_WOOD);
    setWorldObject(2, 6, OBJECT_IRON);
    setWorldObject(3, 6, OBJECT_IRON);
    setWorldObject(4, 6, OBJECT_AXE);
    setWorldObject(5, 6, OBJECT_PICKAXE);
    printf("World generation complete\n");
}

int main(int argc, char **argv)
{
start:
    srand(time(NULL));
    videoSetMode(MODE_0_2D);

    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_SPRITE, VRAM_C_LCD, VRAM_D_LCD);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_512x256, 0, 1);
    dmaCopy(tilemapTiles, bgGetGfxPtr(bg0), tilemapTilesLen);
    dmaCopy(tilemapPal, BG_PALETTE, tilemapPalLen);

    bgSetPriority(bg0, 3);
    bgWrapOn(bg0);

    bg0Map = (uint16_t *)bgGetMapPtr(bg0);

    bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_512x256, 4, 1); // Shared tilemap

    bgSetPriority(bg1, 2);
    bgWrapOn(bg1);

    bg1Map = (uint16_t *)bgGetMapPtr(bg1);

    dmaFillHalfWords(TILE_EMPTY, bg0Map, 64 * 64 * 2);
    dmaFillHalfWords(EMPTY, bg1Map, 64 * 64 * 2);

    oamInit(&oamMain, SpriteMapping_1D_128, false);

    // Allocate space for the tiles and copy them there
    u16 *playerGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(playerTiles, playerGfx, 8 * 8 * 4); // Tile size X * Y * 4 tiles * 2 bytes (u16)
    u16 *cursorGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(uiTiles + 8 * 8 * 4, cursorGfx, 8 * 8 * 4);
    locomotive.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles, locomotive.gfx, 8 * 8 * 4 * 2);
    railStorage.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles + 8 * 8 * 2, railStorage.gfx, 8 * 8 * 4 * 2);
    railBuilder.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles + 8 * 8 * 2 * 2, railBuilder.gfx, 8 * 8 * 4 * 2);

    // Copy palette
    dmaCopy(playerPal, SPRITE_PALETTE, playerPalLen);

    oamSet(&oamMain, 0,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_16x16, SpriteColorFormat_256Color, // Size, format
           playerGfx,                                    // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic
    oamSet(&oamMain, 1,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_16x16, SpriteColorFormat_256Color, // Size, format
           cursorGfx,                                    // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    oamSet(&oamMain, 2,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_32x16, SpriteColorFormat_256Color, // Size, format
           locomotive.gfx,                               // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    oamSet(&oamMain, 3,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_32x16, SpriteColorFormat_256Color, // Size, format
           railStorage.gfx,                              // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    oamSet(&oamMain, 4,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_32x16, SpriteColorFormat_256Color, // Size, format
           railBuilder.gfx,                              // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    consoleDemoInit();

    chunk = -1;

    player.x = TILE_SIZE * 4;
    player.y = TILE_SIZE * 6;
    player.direction = DIR_DOWN;
    player.objectHeld = EMPTY;
    player.quantityHeld = 0;

    for (int i = 0; i < WAGONS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            wagons[i]->slots[j] = EMPTY;
            wagons[i]->quantity[j] = 0;
        }
    }

    int selection = 0;
    int seed;
    while (1)
    {
        swiWaitForVBlank();
        scanKeys();

        printf("\x1b[2J");
        for (int i = 0; i < 2; i++)
        {
            if (i == selection)
                printf("\x1b[%d;0H> ", i);
            else
                printf("\x1b[%d;0H  ", i);
            switch (i)
            {
            case 0:
                printf("Choose Seed\n");
                break;
            case 1:
                printf("Random Seed\n");
                break;
            }
        }

        if (keysDown() & KEY_UP)
        {
            selection--;
            if (selection < 0)
                selection = 1;
        }
        if (keysDown() & KEY_DOWN)
        {
            selection++;
            if (selection > 1)
                selection = 0;
        }
        if (keysDown() & KEY_A)
        {
            if (selection == 0)
            {
                printf("\x1b[4;0HEnter Seed:\nUse arrow keys to change digits, A to confirm\n");
                seed = 0;
                while (1)
                {
                    swiWaitForVBlank();
                    scanKeys();
                    printf("\x1b[8;0H");
                    for (int j = 7; j >= 0; j--)
                    {
                        printf("%X", (seed >> (j * 4)) & 0xF);
                    }
                    printf("\n");
                    for (int j = 0; j < 8; j++)
                    {
                        if (j == selection)
                            printf("^");
                        else
                            printf(" ");
                    }
                    if (keysDown() & KEY_LEFT)
                    {
                        selection--;
                        if (selection < 0)
                            selection = 7;
                    }
                    else if (keysDown() & KEY_RIGHT)
                    {
                        selection++;
                        if (selection > 7)
                            selection = 0;
                    }
                    else if (keysDown() & (KEY_UP | KEY_DOWN))
                    {
                        int shift = (7 - selection) * 4;
                        int digit = (seed >> shift) & 0xF;

                        if (keysDown() & KEY_UP)
                            digit = (digit + 1) & 0xF;
                        else
                            digit = (digit - 1) & 0xF;

                        seed &= ~(0xF << shift);
                        seed |= (digit << shift);
                    }
                    else if (keysDown() & KEY_A)
                    {
                        goto generate;
                    }
                }
            }
            else
            {
                seed = rand() % 0xFFFFFFFF;
                goto generate;
            }
        }
    }

generate:

    generateWorld(seed);

    locomotive.x = 32;
    locomotive.y = 4.8 * TILE_SIZE;

    while (1)
    {
        swiWaitForVBlank();

        scanKeys();

        int held = keysHeld();
        int down = keysDown();
        if (held & KEY_START)
            break;

        int newX = player.x;
        int newY = player.y;

        if (held & KEY_UP)
        {
            newY--;
            player.direction = DIR_UP;
            dmaCopy(playerTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4);
        }
        if (held & KEY_DOWN)
        {
            newY++;
            player.direction = DIR_DOWN;
            dmaCopy(playerTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4);
        }
        if (held & KEY_LEFT)
        {
            newX--;
            player.direction = DIR_LEFT;
            dmaCopy(playerTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4);
        }
        if (held & KEY_RIGHT)
        {
            newX++;
            player.direction = DIR_RIGHT;
            dmaCopy(playerTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4);
        }
        if (!checkCollision(newX, player.y))
            player.x = newX;

        if (!checkCollision(player.x, newY))
            player.y = newY;

        if (frames % 60 == 0)
        {
            if (player.objectHeld == OBJECT_AXE)
            {
                if (player.direction == DIR_UP)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE - 1] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE - 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE - 1] - 1);
                    }
                }
                else if (player.direction == DIR_DOWN)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE + 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] - 1);
                    }
                }
                else if (player.direction == DIR_LEFT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE - 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] - 1);
                    }
                }
                else if (player.direction == DIR_RIGHT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE + 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] - 1);
                    }
                }
            }
            else if (player.objectHeld == OBJECT_PICKAXE)
            {
                if (player.direction == DIR_UP)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE - 1] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE - 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE - 1] - 1);
                    }
                }
                else if (player.direction == DIR_DOWN)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE + 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] - 1);
                    }
                }
                else if (player.direction == DIR_LEFT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE - 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] - 1);
                    }
                }
                else if (player.direction == DIR_RIGHT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE + 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] - 1);
                    }
                }
            }

            if (railStorage.quantity[0] && railStorage.quantity[1] && railBuilder.quantity[0] < railBuilder.maxQuantity)
            {
                // Take one wood and one iron from storage and turn them into a rail through the builder
                railBuilder.slots[0] = OBJECT_RAIL;
                railBuilder.quantity[0]++;
                updateWagon(2);

                railStorage.quantity[0]--;
                railStorage.quantity[1]--;
                updateWagon(1);
            }
        }

        int targetWagonX, targetWagonY;

        if (player.direction == DIR_UP)
        {
            targetWagonX = (int)player.x + 8;
            targetWagonY = (int)player.y - 8;
        }
        else if (player.direction == DIR_DOWN)
        {
            targetWagonX = (int)player.x + 8;
            targetWagonY = (int)player.y + PLAYER_SIZE + 8;
        }
        else if (player.direction == DIR_LEFT)
        {
            targetWagonX = (int)player.x - 8;
            targetWagonY = (int)player.y + 8;
        }
        else // DIR_RIGHT
        {
            targetWagonX = (int)player.x + PLAYER_SIZE + 8;
            targetWagonY = (int)player.y + 8;
        }

        player.selectedWagon = false;
        for (int i = 0; i < WAGONS; i++)
        {
            struct Wagon *wagon = wagons[i];
            if (targetWagonY >= wagon->y && targetWagonY < wagon->y + wagon->sizeY &&
                targetWagonX >= wagon->x && targetWagonX < wagon->x + wagon->sizeX)
            {
                if (targetWagonX >= wagon->x &&
                    targetWagonX < wagon->x + wagon->sizeX / 2 &&
                    wagon->acceptedObjects[0] == player.objectHeld &&
                    wagon->acceptedObjects[0] != EMPTY &&
                    wagon->quantity[0] < wagon->maxQuantity)
                {
                    player.selectedWagonSlot = 0;
                    player.selectedWagonId = i;
                    player.selectedWagon = true;
                }
                else if (targetWagonX >= wagon->x + wagon->sizeX / 2 &&
                         targetWagonX < wagon->x + wagon->sizeX &&
                         wagon->acceptedObjects[1] == player.objectHeld &&
                         wagon->acceptedObjects[1] != EMPTY &&
                         wagon->quantity[1] < wagon->maxQuantity)
                {
                    player.selectedWagonSlot = 1;
                    player.selectedWagonId = i;
                    player.selectedWagon = true;
                }
                else if (targetWagonX >= wagon->x &&
                         targetWagonX < wagon->x + wagon->sizeX / 2 &&
                         player.objectHeld == EMPTY &&
                         wagon->acceptedObjects[0] == EMPTY &&
                         wagon->maxQuantity > 0 &&
                         wagon->quantity[0] > 0 &&
                         wagon->slots[0] != EMPTY) // slots with no accepted objects but a maximum of objects means its an output slot
                {
                    player.selectedWagonSlot = 0;
                    player.selectedWagonId = i;
                    player.selectedWagon = true;
                }
            }
        }

        // Select object automatically

        player.selectedObjectX = (player.x + 8) / TILE_SIZE;
        player.selectedObjectY = (player.y + 8) / TILE_SIZE;

        if (player.selectedObjectX != lastPlacedX || player.selectedObjectY != lastPlacedY)
            justPlaced = false;

        // Automatic pickup if same thing held and selected
        if (player.objectHeld == worldObjects[player.selectedObjectX][player.selectedObjectY] &&
            player.quantityHeld < player.maxQuantityHeld &&
            justPlaced == false &&
            worldObjects[player.selectedObjectX][player.selectedObjectY] != EMPTY)
        {
            player.quantityHeld++;
            setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
        }

        if (worldObjects[player.selectedObjectX][player.selectedObjectY] != EMPTY && !player.selectedWagon)
        {
            if (worldObjects[player.selectedObjectX][player.selectedObjectY] == OBJECT_RAIL)
            {
                //   x
                // x R x      Check these tiles and make sure the rail isnt intermediate between two other rails
                //   x        if it is, then it is not selected
                if ((player.selectedObjectX >= locomotive.x / TILE_SIZE &&
                     player.selectedObjectX <= (locomotive.x + locomotive.sizeX) / TILE_SIZE) ||

                    (player.selectedObjectX == 0 &&
                     worldObjects[player.selectedObjectX + 1][player.selectedObjectY] == OBJECT_RAIL) ||

                    (worldObjects[player.selectedObjectX - 1][player.selectedObjectY] == OBJECT_RAIL &&
                     worldObjects[player.selectedObjectX + 1][player.selectedObjectY] == OBJECT_RAIL))

                    player.selectedObject = false;
                else
                    player.selectedObject = true;
            }
            else
                player.selectedObject = true;
        }
        else
        {
            player.selectedObject = false;
        }

        if (down & KEY_A)
        {
            if (player.objectHeld != EMPTY)
            {
                if (player.selectedObject)
                {
                    if (player.quantityHeld == 1 && player.objectHeld != worldObjects[player.selectedObjectX][player.selectedObjectY])
                    {
                        // Swap object
                        int temp = worldObjects[player.selectedObjectX][player.selectedObjectY];
                        setWorldObject(player.selectedObjectX, player.selectedObjectY, player.objectHeld);
                        player.objectHeld = temp;
                    }
                }
                else if (player.selectedWagon)
                {
                    // Put object in wagon
                    wagons[player.selectedWagonId]->slots[player.selectedWagonSlot] = player.objectHeld;
                    while (player.quantityHeld > 0 && wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot] < wagons[player.selectedWagonId]->maxQuantity)
                    {
                        wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot]++;
                        player.quantityHeld--;
                    }

                    updateWagon(player.selectedWagonId);
                }
                else
                {
                    // Place object
                    setWorldObject(player.selectedObjectX, player.selectedObjectY, player.objectHeld);
                    player.quantityHeld--;
                    lastPlacedX = player.selectedObjectX;
                    lastPlacedY = player.selectedObjectY;
                    justPlaced = true;
                }
            }
            else if (player.selectedWagon) // Playing is selecting an output wagon
            {
                player.objectHeld = wagons[player.selectedWagonId]->slots[player.selectedWagonSlot];
                while (wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot] > 0 && player.quantityHeld < player.maxQuantityHeld)
                {
                    wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot]--;
                    player.quantityHeld++;
                }

                updateWagon(player.selectedWagonId);
            }
            else if (player.selectedObject)
            {
                // Pick up object
                player.objectHeld = worldObjects[player.selectedObjectX][player.selectedObjectY];
                setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
                player.quantityHeld = 1;
            }
        }

        if (player.quantityHeld == 0)
            player.objectHeld = EMPTY;

        // test for locomotive derailment
        if (locomotive.direction == DIR_RIGHT)
        {
            if (worldObjects[(int)(locomotive.x + locomotive.sizeX + locomotive.speed) / TILE_SIZE][(int)(locomotive.y + locomotive.sizeY) / TILE_SIZE] != OBJECT_RAIL)
                goto start;
            else
            {
                locomotive.x += locomotive.speed;
                if (locomotive.x + locomotive.sizeX >= player.x && locomotive.x + locomotive.sizeX < player.x + PLAYER_SIZE &&
                    ((locomotive.y >= player.y && locomotive.y < player.y + PLAYER_SIZE) ||
                     (locomotive.y + locomotive.sizeY > player.y && locomotive.y + locomotive.sizeY <= player.y + PLAYER_SIZE))) // The front of the locomotive is colliding with the player
                    player.x++;
            }
        }
        railStorage.x = locomotive.x - railStorage.sizeX;
        railStorage.y = locomotive.y;

        railBuilder.x = railStorage.x - railBuilder.sizeX;
        railBuilder.y = railStorage.y;

        if (locomotive.x + locomotive.sizeX >= WORLD_WIDTH * TILE_SIZE - SCREEN_WIDTH / 2)
            scroll = WORLD_WIDTH * TILE_SIZE - SCREEN_WIDTH;
        else if (locomotive.x + locomotive.sizeX >= SCREEN_WIDTH / 2)
            scroll = locomotive.x + locomotive.sizeX - SCREEN_WIDTH / 2;
        else
            scroll = 0;

        // Chunking (btw, player never goes back to left, like Super Mario Bros NES)
        if ((scroll + SCREEN_WIDTH) / TILE_SIZE >= chunk * 4)
        {
            chunk++;
            if (chunk < WORLD_WIDTH / 4 - 1)
            {
                for (int x = chunk * 4; x < chunk * 4 + 4; x++)
                {
                    for (int y = 0; y < WORLD_HEIGHT; y++)
                    {
                        bg0SetTile((x * 2) % 64, y * 2, worldTerrain[x][y] * 4 + worldVariants[x][y] * 16 * 4);
                        bg0SetTile((x * 2) % 64 + 1, y * 2, worldTerrain[x][y] * 4 + 1 + worldVariants[x][y] * 16 * 4);
                        bg0SetTile((x * 2) % 64, y * 2 + 1, worldTerrain[x][y] * 4 + 2 + worldVariants[x][y] * 16 * 4);
                        bg0SetTile((x * 2) % 64 + 1, y * 2 + 1, worldTerrain[x][y] * 4 + 3 + worldVariants[x][y] * 16 * 4);
                        bg1SetTile((x * 2) % 64, y * 2, worldObjects[x][y] * 4);
                        bg1SetTile((x * 2) % 64 + 1, y * 2, worldObjects[x][y] * 4 + 1);
                        bg1SetTile((x * 2) % 64, y * 2 + 1, worldObjects[x][y] * 4 + 2);
                        bg1SetTile((x * 2) % 64 + 1, y * 2 + 1, worldObjects[x][y] * 4 + 3);
                    }
                }
            }
        }

        if (player.selectedObjectX >= scroll / TILE_SIZE &&
            player.selectedObjectX < (scroll + SCREEN_WIDTH) / TILE_SIZE)
        {
            if (player.selectedObject)
                oamSetXY(&oamMain, 1, player.selectedObjectX * TILE_SIZE - scroll, player.selectedObjectY * TILE_SIZE);
            else if (player.selectedWagon)
                oamSetXY(&oamMain, 1, wagons[player.selectedWagonId]->x + player.selectedWagonSlot * TILE_SIZE - scroll, wagons[player.selectedWagonId]->y);
            else
                oamSetXY(&oamMain, 1, -16, -16);
        }
        else
            oamSetXY(&oamMain, 1, -16, -16);

        if (player.x >= scroll - TILE_SIZE && player.x < scroll + SCREEN_WIDTH)
            oamSetXY(&oamMain, 0, player.x - scroll, player.y);
        else
            oamSetXY(&oamMain, 0, -16, -16);

        for (int i = 0; i < WAGONS; i++)
        {
            oamSetXY(&oamMain, i + 2, wagons[i]->x - scroll, wagons[i]->y);
        }

        bgSetScroll(bg0, scroll, 0);
        bgSetScroll(bg1, scroll, 0);

        bgUpdate();
        oamUpdate(&oamMain);

        printf("\x1b[2J");
        printf("x: %f, y: %f, dir: %d, obj: %d\n", player.x, player.y, player.direction, player.selectedObject);
        printf("x: %d, y: %d, object: %d\n", player.selectedObjectX, player.selectedObjectY, worldObjects[player.selectedObjectX][player.selectedObjectY]);
        printf("obj held: %d, quantity: %d\n", player.objectHeld, player.quantityHeld);
        printf("chunk: %d, scroll: %d\n", chunk, scroll);

        frames++;
    }

    return 0;
}
