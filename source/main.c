#include <stdio.h>
#include <nds.h>

#include "tilemap.h"
#include "player.h"
#include "ui.h"
#include "wagons.h"

#define WORLD_WIDTH 16
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

int bg0;
uint16_t *bg0Map;
int bg1;
uint16_t *bg1Map;

unsigned int frames = 0;

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
};

struct Wagon
{
    float x;
    float y;
    int sizeX;
    int sizeY;
    int direction;
    float speed;
};

struct Player player = {WORLD_WIDTH * TILE_SIZE / 2, WORLD_HEIGHT *TILE_SIZE / 2, DIR_DOWN, 0, 0, 3, 0, 0, false};

struct Wagon locomotive = {0, 0, 32, 16, DIR_RIGHT, 0.01};

uint8_t worldTerrain[WORLD_WIDTH][WORLD_HEIGHT];
uint8_t worldObjects[WORLD_WIDTH][WORLD_HEIGHT];
uint8_t worldHealth[WORLD_WIDTH][WORLD_HEIGHT];

void bg0SetTile(int x, int y, int tile)
{
    if (x < 0 || x >= 32 || y < 0 || y >= 32)
        return;
    bg0Map[x + y * 32] = tile;
}
void bg1SetTile(int x, int y, int tile)
{
    if (x < 0 || x >= 32 || y < 0 || y >= 32)
        return;
    bg1Map[x + y * 32] = tile;
}

void setWorldTile(int x, int y, int tile)
{
    worldTerrain[x][y] = tile;
    worldHealth[x][y] = 3;
    bg0SetTile(x * 2, y * 2, tile * 4);
    bg0SetTile(x * 2 + 1, y * 2, tile * 4 + 1);
    bg0SetTile(x * 2, y * 2 + 1, tile * 4 + 2);
    bg0SetTile(x * 2 + 1, y * 2 + 1, tile * 4 + 3);
}

void setWorldObject(int x, int y, int tile)
{
    worldObjects[x][y] = tile;
    bg1SetTile(x * 2, y * 2, tile * 4);
    bg1SetTile(x * 2 + 1, y * 2, tile * 4 + 1);
    bg1SetTile(x * 2, y * 2 + 1, tile * 4 + 2);
    bg1SetTile(x * 2 + 1, y * 2 + 1, tile * 4 + 3);
}

void setWorldHealth(int x, int y, int health)
{
    worldHealth[x][y] = health;
    if (worldHealth[x][y] == 0) {
        if (worldTerrain[x][y] == TILE_TREE)
            setWorldObject(x, y, OBJECT_WOOD);
        else if (worldTerrain[x][y] == TILE_ROCK)
            setWorldObject(x, y, OBJECT_IRON);
        setWorldTile(x, y, TILE_EMPTY);
        return;
    }
    int tile = worldTerrain[x][y] + (3 - health) * 2;
    bg1SetTile(x * 2, y * 2, tile * 4);
    bg1SetTile(x * 2 + 1, y * 2, tile * 4 + 1);
    bg1SetTile(x * 2, y * 2 + 1, tile * 4 + 2);
    bg1SetTile(x * 2 + 1, y * 2 + 1, tile * 4 + 3);
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

    if ((newX >= locomotive.x && newX < locomotive.x + locomotive.sizeX &&
         newY >= locomotive.y && newY < locomotive.y + locomotive.sizeY) ||
        (newX + PLAYER_SIZE >= locomotive.x && newX + PLAYER_SIZE < locomotive.x + locomotive.sizeX &&
         newY >= locomotive.y && newY < locomotive.y + locomotive.sizeY) ||
        (newX >= locomotive.x && newX < locomotive.x + locomotive.sizeX &&
         newY + PLAYER_SIZE >= locomotive.y && newY + PLAYER_SIZE < locomotive.y + locomotive.sizeY) ||
        (newX + PLAYER_SIZE >= locomotive.x && newX + PLAYER_SIZE < locomotive.x + locomotive.sizeX &&
         newY + PLAYER_SIZE >= locomotive.y && newY + PLAYER_SIZE < locomotive.y + locomotive.sizeY))
        return true;

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

int main(int argc, char **argv)
{
start:
    videoSetMode(MODE_0_2D);

    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_SPRITE, VRAM_C_LCD, VRAM_D_LCD);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_512x512, 0, 1);
    dmaCopy(tilemapTiles, bgGetGfxPtr(bg0), tilemapTilesLen);
    dmaCopy(tilemapPal, BG_PALETTE, tilemapPalLen);

    bgSetPriority(bg0, 3);

    bg0Map = (uint16_t *)bgGetMapPtr(bg0);

    bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_512x512, 4, 1); // Shared tilemap

    bgSetPriority(bg1, 2);

    bg1Map = (uint16_t *)bgGetMapPtr(bg1);

    dmaFillHalfWords(TILE_EMPTY, bg0Map, 64 * 64 * 2);
    dmaFillHalfWords(EMPTY, bg1Map, 64 * 64 * 2);

    oamInit(&oamMain, SpriteMapping_1D_128, false);

    // Allocate space for the tiles and copy them there
    u16 *playerGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(playerTiles, playerGfx, 8 * 8 * 4); // Tile size X * Y * 4 tiles * 2 bytes (u16)
    u16 *cursorGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(uiTiles + 8 * 8 * 4, cursorGfx, 8 * 8 * 4);
    u16 *locomotiveGfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles, locomotiveGfx, 8 * 8 * 4 * 2);

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
           locomotiveGfx,                                // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    consoleDemoInit();

    printf("Derailed by AzizBgBoss\n");

    printf("Press START to exit to loader\n");

    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            setWorldTile(x, y, TILE_TREE);
        }

        for (int y = 3; y < WORLD_HEIGHT - 3; y++)
        {
            setWorldTile(x, y, TILE_EMPTY);
        }

        for (int y = WORLD_HEIGHT - 3; y < WORLD_HEIGHT; y++)
        {
            setWorldTile(x, y, TILE_ROCK);
        }

        for (int y = 0; y < WORLD_HEIGHT; y++)
        {
            setWorldObject(x, y, EMPTY);
        }
    }

    for (int x = 0; x < 5; x++)
    {
        setWorldObject(x, 5, OBJECT_RAIL);
    }

    for (int x = 0; x < WORLD_WIDTH / 2; x++)
    {
        setWorldObject(x * 2, 6, OBJECT_WOOD);
        setWorldObject(x * 2 + 1, 7, OBJECT_IRON);
    }

    setWorldObject(0, 3, OBJECT_AXE);
    setWorldObject(1, 3, OBJECT_PICKAXE);

    locomotive.x = 0;
    locomotive.y = 5 * TILE_SIZE;

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
        }

        // Select object automatically

        player.selectedObjectX = (player.x + 8) / TILE_SIZE;
        player.selectedObjectY = (player.y + 8) / TILE_SIZE;

        if (player.selectedObjectX != lastPlacedX || player.selectedObjectY != lastPlacedY)
            justPlaced = false;

        // Automatic pickup if same thing held and selected
        if (player.objectHeld == worldObjects[player.selectedObjectX][player.selectedObjectY] &&
            player.quantityHeld < player.maxQuantityHeld &&
            justPlaced == false)
        {
            player.quantityHeld++;
            setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
        }

        if (worldObjects[player.selectedObjectX][player.selectedObjectY] != EMPTY)
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
            if (worldObjects[(int)(locomotive.x + locomotive.sizeX + locomotive.speed) / TILE_SIZE][(int)locomotive.y / TILE_SIZE] != OBJECT_RAIL)
                goto start;
            else
            {
                locomotive.x += locomotive.speed;
                if (locomotive.x + locomotive.sizeX >= player.x && locomotive.x + locomotive.sizeX < player.x + PLAYER_SIZE &&
                    ((locomotive.y >= player.y && locomotive.y < player.y + PLAYER_SIZE) ||
                     (locomotive.y + locomotive.sizeY >= player.y && locomotive.y + locomotive.sizeY < player.y + PLAYER_SIZE))) // The front of the locomotive is colliding with the player
                    player.x++;
            }
        }

        if (player.selectedObject)
            oamSetXY(&oamMain, 1, player.selectedObjectX * TILE_SIZE, player.selectedObjectY * TILE_SIZE);
        else
            oamSetXY(&oamMain, 1, -16, -16);

        oamSetXY(&oamMain, 0, player.x, player.y);
        oamSetXY(&oamMain, 2, locomotive.x, locomotive.y);

        oamUpdate(&oamMain);

        printf("\x1b[2J");
        printf("x: %f, y: %f, dir: %d, obj: %d\n", player.x, player.y, player.direction, player.selectedObject);
        printf("x: %d, y: %d, object: %d\n", player.selectedObjectX, player.selectedObjectY, worldObjects[player.selectedObjectX][player.selectedObjectY]);
        printf("obj held: %d, quantity: %d\n", player.objectHeld, player.quantityHeld);

        frames++;
    }

    return 0;
}
