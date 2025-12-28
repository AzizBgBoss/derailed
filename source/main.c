#include <stdio.h>
#include <nds.h>

int main(int argc, char **argv)
{
    consoleDemoInit();

    printf("Derailed by AzizBgBoss\n");

    while (1)
    {
        swiWaitForVBlank();

        scanKeys();
        if (keysHeld() & KEY_START)
            break;
    }

    return 0;
}
