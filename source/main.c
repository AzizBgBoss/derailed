#include <stdio.h>
#include <nds.h>

#include "tilemap.h"

#define WORLD_WIDTH 16
#define WORLD_HEIGHT 12

#define TILE_EMPTY 0
#define TILE_TREE 1
#define TILE_ROCK 2

int bg0;
uint16_t *bg0Map;

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

    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_LCD, VRAM_C_LCD, VRAM_D_LCD);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);

    dmaCopy(tilemapTiles, bgGetGfxPtr(bg0), tilemapTilesLen);
    dmaCopy(tilemapPal, BG_PALETTE, tilemapPalLen);

    bg0Map = (uint16_t *)bgGetMapPtr(bg0);

    dmaFillHalfWords(TILE_EMPTY, bg0Map, 32 * 32 * 2);

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
        if (keysHeld() & KEY_START)
            break;

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
