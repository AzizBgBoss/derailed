#include <stdio.h>
#include <nds.h>

#include "tilemap.h"
#include "sprites.h"

#define WORLD_WIDTH 16
#define WORLD_HEIGHT 12
#define TILE_SIZE 16

#define TILE_EMPTY 0
#define TILE_TREE 1
#define TILE_ROCK 2

#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3

int bg0;
uint16_t *bg0Map;

struct Player
{
    int x;
    int y;
    int direction;
};

struct Player player = {0, 0, DIR_DOWN};

uint8_t worldTerrain[WORLD_WIDTH][WORLD_HEIGHT];

void bg0SetTile(int x, int y, int tile)
{
    if (x < 0 || x >= 32 || y < 0 || y >= 32)
        return;
    bg0Map[x + y * 32] = tile;
}

void setWorldTile(int x, int y, int tile)
{
    worldTerrain[x][y] = tile;
    bg0SetTile(x * 2, y * 2, tile * 4);
    bg0SetTile(x * 2 + 1, y * 2, tile * 4 + 1);
    bg0SetTile(x * 2, y * 2 + 1, tile * 4 + 2);
    bg0SetTile(x * 2 + 1, y * 2 + 1, tile * 4 + 3);
}

int main(int argc, char **argv)
{
    videoSetMode(MODE_0_2D);

    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_SPRITE, VRAM_C_LCD, VRAM_D_LCD);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_512x512, 0, 1);
    dmaCopy(tilemapTiles, bgGetGfxPtr(bg0), tilemapTilesLen);
    dmaCopy(tilemapPal, BG_PALETTE, tilemapPalLen);

    bgSetPriority(bg0, 3);

    bg0Map = (uint16_t *)bgGetMapPtr(bg0);

    dmaFillHalfWords(TILE_EMPTY, bg0Map, 32 * 32 * 2);

    oamInit(&oamMain, SpriteMapping_1D_128, false);

    // Allocate space for the tiles and copy them there
    u16 *playerGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(spritesTiles, playerGfx, 8 * 8 * 4 * 2); // Tile size X * Y * 4 tiles * 2 bytes (u16)

    // Copy palette
    dmaCopy(spritesPal, SPRITE_PALETTE, spritesPalLen);

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
    }

    while (1)
    {
        swiWaitForVBlank();

        scanKeys();

        int held = keysHeld();
        if (held & KEY_START)
            break;

        if (held & KEY_UP)
        {
            player.y--;
            player.direction = DIR_UP;
            dmaCopy(spritesTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4 * 2);
        }
        if (held & KEY_DOWN)
        {
            player.y++;
            player.direction = DIR_DOWN;
            dmaCopy(spritesTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4 * 2);
        }
        if (held & KEY_LEFT)
        {
            player.x--;
            player.direction = DIR_LEFT;
            dmaCopy(spritesTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4 * 2);
        }
        if (held & KEY_RIGHT)
        {
            player.x++;
            player.direction = DIR_RIGHT;
            dmaCopy(spritesTiles + 8 * 8 * 4 * player.direction, playerGfx, 8 * 8 * 4 * 2);
        }

        if (player.x < 0)
            player.x = 0;
        if (player.x >= WORLD_WIDTH * TILE_SIZE - TILE_SIZE)
            player.x = WORLD_WIDTH * TILE_SIZE - TILE_SIZE - 1;
        if (player.y < 0)
            player.y = 0;
        if (player.y >= WORLD_HEIGHT * TILE_SIZE - TILE_SIZE)
            player.y = WORLD_HEIGHT * TILE_SIZE - TILE_SIZE - 1;

        oamSetXY(&oamMain, 0, player.x, player.y);
        oamUpdate(&oamMain);

        printf("\x1b[3;0H");
        for (int y = 0; y < WORLD_HEIGHT; y++)
        {
            for (int x = 0; x < WORLD_WIDTH; x++)
            {
                switch (worldTerrain[x][y])
                {
                case TILE_EMPTY:
                    printf(" ");
                    break;
                case TILE_TREE:
                    printf("T");
                    break;
                case TILE_ROCK:
                    printf("R");
                    break;
                }
            }
            printf("\n");
        }
    }

    return 0;
}
