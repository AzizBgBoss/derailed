#include <stdio.h>
#include <nds.h>
#include <time.h>
#include <dswifi9.h>
#include <maxmod9.h>
#include <filesystem.h>

#include "perlin.h"
#include "pathfinder.h" // "defs.h" is included here

#include "tilemap.h"
#include "player.h"
#include "player2.h"
#include "robot.h"
#include "ui.h"
#include "wagons.h"
#include "intro.h"

#include "soundbank.h"

// TODO: My code is not clean, i need to get rid of repeated expressions and split the code into functions
// There's also a lot of magic numbers, i need to define them

int bg0;
uint16_t *bg0Map;
int bg1;
uint16_t *bg1Map;

unsigned int frames = 0;

bool debugMode = false;
int gameMode = GAMEMODE_SINGLEPLAYER;
uint8_t gameStarted = 0;
uint8_t gamePlayerMask = 0;
uint8_t lastId = 0;
bool doneUpdate = false;
uint8_t lastReceivedId = 255;

static Wifi_AccessPoint AccessPoint;

int scroll;
int chunk; // Current chunk loaded (each chunk is 4 tiles wide)
int seed;
int worldPart;

int lastPlacedX;
int lastPlacedY;
bool justPlaced;

bool interact;

int path[32];
int len = 0;
int target = -1;
int searchStep = SEARCH_IDLE;
int searchObject = OBJECT_PICKAXE;
uint8_t bobotEnergy = 100;
char bobotMessage[128];

struct Update
{
    bool occupied;
    uint8_t id;
    uint8_t x;
    uint8_t y;
    uint8_t action;
    uint8_t parameter;
};

struct Player
{
    u16 *gfx;
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
    int animationFrame;
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

struct Player player = {NULL, 0, 0, DIR_DOWN, EMPTY, 0, 3, 0, 0, false, 0, 0, false, 0};

struct Player player2;

struct Wagon locomotive = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.01f, {EMPTY, EMPTY}, {0, 0}, 0, {EMPTY, EMPTY}};
struct Wagon railStorage = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.0f, {EMPTY, EMPTY}, {0, 0}, 3, {OBJECT_IRON, OBJECT_WOOD}};
struct Wagon railBuilder = {NULL, 0, 0, 32, 16, DIR_RIGHT, 0.0f, {EMPTY, EMPTY}, {0, 0}, 3, {EMPTY, EMPTY}};

#define WAGONS 3

struct Wagon *wagons[WAGONS] = {&locomotive, &railStorage, &railBuilder};

struct Update updates[MAX_UPDATES];

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

void delay(float seconds)
{
    int start = time(NULL);
    while (time(NULL) - start < seconds)
    {
        swiWaitForVBlank();
    }
}

bool queueUpdate(int x, int y, int action, int parameter)
{
    if (gameMode != GAMEMODE_SINGLEPLAYER)
    {
        for (int i = 0; i < MAX_UPDATES; i++)
        {
            if (!updates[i].occupied)
            {
                updates[i].occupied = true;
                updates[i].id = lastId++;
                updates[i].x = x;
                updates[i].y = y;
                updates[i].action = action;
                updates[i].parameter = parameter;
                return true;
            }
        }
        return false;
    }
    return false;
}

void setWorldTile(int x, int y, int tile)
{
    if (gameMode != GAMEMODE_CLIENT)
    {
        worldTerrain[x][y] = tile;
        worldHealth[x][y] = 3;
        worldVariants[x][y] = hash((x << 16) | y, seed) % 4;

        if (x >= (chunk - 5) * 4 && x <= chunk * 4)
        {
            bg0SetTile((x * 2) % 64, y * 2, tile * 4 + worldVariants[x][y] * 16 * 4);
            bg0SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1 + worldVariants[x][y] * 16 * 4);
            bg0SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2 + worldVariants[x][y] * 16 * 4);
            bg0SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3 + worldVariants[x][y] * 16 * 4);
        }
    }

    queueUpdate(x, y, ACTION_SETWORLDTERRAIN, tile);
}

void setWorldObject(int x, int y, int tile)
{
    if (gameMode != GAMEMODE_CLIENT)
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

    queueUpdate(x, y, ACTION_SETWORLDOBJECT, tile);
}

void setWorldHealth(int x, int y, int health)
{
    if (gameMode != GAMEMODE_CLIENT)
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

    queueUpdate(x, y, ACTION_SETWORLDHEALTH, health);
}

void updateWagon(int id)
{
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[0], wagons[id]->gfx, 8 * 8 * 2);                      // Top-left quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[1] + 8 * 4, wagons[id]->gfx + 8 * 8, 8 * 8 * 2);      // Top-right quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[0] + 8 * 8, wagons[id]->gfx + 8 * 8 * 2, 8 * 8 * 2);  // Bottom-left quarter
    dmaCopy(wagonsTiles + 8 * 8 * 2 * id + 8 * 8 * 2 * WAGONS * wagons[id]->quantity[1] + 8 * 12, wagons[id]->gfx + 8 * 8 * 3, 8 * 8 * 2); // Bottom-right quarter
}

void setWagonObject(int wagonId, int slot, int object)
{

    if (gameMode != GAMEMODE_CLIENT)
    {
        wagons[wagonId]->slots[slot] = object;
        updateWagon(wagonId);
    }

    queueUpdate(wagonId, slot, ACTION_SETWAGONOBJECT, object);
}

void setWagonQuantity(int wagonId, int slot, int quantity)
{
    if (gameMode != GAMEMODE_CLIENT)
    {
        wagons[wagonId]->quantity[slot] = quantity;
        updateWagon(wagonId);
    }

    queueUpdate(wagonId, slot, ACTION_SETWAGONQUANTITY, quantity);
}

void setPlayerObjectHeld(int object)
{
    if (gameMode != GAMEMODE_CLIENT)
    {
        player.objectHeld = object;
    }

    queueUpdate(0, 0, ACTION_SETPLAYEROBJECTHELD, object);
}

void setPlayerQuantity(int quantity)
{
    if (gameMode != GAMEMODE_CLIENT)
    {
        player.quantityHeld = quantity;
        if (player.quantityHeld == 0)
            player.objectHeld = EMPTY;
    }

    queueUpdate(0, 0, ACTION_SETPLAYERQUANTITY, quantity);
}

const char *getObjectName(int object)
{
    switch (object)
    {
    case OBJECT_RAIL:
        return "Rail";
    case OBJECT_WOOD:
        return "Wood";
    case OBJECT_IRON:
        return "Iron";
    case OBJECT_AXE:
        return "Axe";
    case OBJECT_PICKAXE:
        return "Pickaxe";
    default:
        return "";
    }
}

const char *getObjectDescription(int object)
{
    switch (object)
    {
    case OBJECT_RAIL:
        return "Put rails in front of the locomotive to build tracks and avoid derailing it and losing.";
    case OBJECT_WOOD:
        return "Put wood in the storage wagon along with iron to build tracks.";
    case OBJECT_IRON:
        return "Put iron in the storage wagon along with wood to build tracks.";
    case OBJECT_AXE:
        return "Use them to chop trees and obtain wood.";
    case OBJECT_PICKAXE:
        return "Use them to mine rocks and obtain iron.";
    default:
        return "";
    }
}

const char *acknowledgement[] = {
    "Okay,",
    "Got it,",
    "Yessir,",
    "Okie dokie!",
    "Roger that,",
    "On it,",
    "Copy that,",
    "Affirmative,",
    "As you wish,",
    "If you insist,",
};

const char *searching[] = {
    "I'm looking for the",
    "Getting the",
    "Searching for the",
    "Hunting down the",
    "Tracking the",
    "Locating the",
    "Scanning for the",
    "Trying to find the",
};

const char *found[] = {
    "Found it!",
    "Target found!",
    "Found the target!",
    "Target located!",
    "Got it!",
    "I have it!",
    "Here it is!",
    "Objective secured!",
};

const char *returning[] = {
    "Bringing you the",
    "I'm coming with the",
    "Delivering the",
    "Heading back with the",
    "On my way with the",
    "Returning with the",
};

const char *idle[] = {
    "Just chilling...",
    "I'm just chilling...",
    "I'm bored...",
    "I wonder what food tastes like...",
    "You do this for a living?",
    "Waiting for you to give me another task!",
    "Standing by...",
    "Nothing to do but wait...",
    "Existence is pain.",
    "Still here.",
    "I could be optimizing something...",
    "No tasks detected.",
};

const char *blocked[] = {
    "I can't reach that.",
    "Path is blocked!",
    "Something's in the way.",
    "That's not accessible.",
    "Obstacle detected.",
    "I fear that I can't get there.",
};

void setBobotMessage(int type, int object)
{
    if (type == SEARCH_SEARCHING)
    {
        sprintf(bobotMessage, "%s %s %s...", acknowledgement[rand() % (sizeof(acknowledgement) / sizeof(acknowledgement[0]))], searching[rand() % (sizeof(searching) / sizeof(searching[0]))], getObjectName(object));
    }
    else if (type == SEARCH_RETURNING)
    {
        sprintf(bobotMessage, "%s %s %s...", found[rand() % (sizeof(found) / sizeof(found[0]))], returning[rand() % (sizeof(returning) / sizeof(returning[0]))], getObjectName(object));
    }
    else if (type == SEARCH_IDLE)
    {
        sprintf(bobotMessage, "%s", idle[rand() % (sizeof(idle) / sizeof(idle[0]))]);
    }
    else if (type == SEARCH_BLOCKED)
    {
        sprintf(bobotMessage, "%s", blocked[rand() % (sizeof(blocked) / sizeof(blocked[0]))]);
    }
}

void setBobotSearch(int type, int object)
{
    searchStep = type;
    searchObject = object;

    setBobotMessage(type, object);
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

void generateWorld(int seed)
{
    uint8_t temp = gameMode;
    gameMode = GAMEMODE_SINGLEPLAYER; // Temporarily disable networking during world generation
    printf("\x1b[2J");
    if (worldPart == 0)
        printf("Generating world with seed %d\n", seed);
    else
        printf("Generating world part %d with seed %d\n", worldPart + 1, seed);

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
            float noiseValue = fractalPerlin2D(x + WORLD_WIDTH * worldPart, y, 4, 0.5f, 0.1f, seed);
            if (noiseValue < -0.15f)
            {
                setWorldTile(x, y, TILE_ROCK);
            }
            else if (noiseValue < 0.15f)
            {
                setWorldTile(x, y, TILE_EMPTY);
            }
            else
            {
                setWorldTile(x, y, TILE_TREE);
            }
        }
    }

    for (int x = 0; x < 8; x++)
    {
        setWorldTile(x, RAILS_Y, TILE_EMPTY);
        setWorldObject(x, RAILS_Y, OBJECT_RAIL);
        setWorldTile(x, RAILS_Y + 1, TILE_EMPTY);
    }

    setWorldObject(0, RAILS_Y + 1, OBJECT_WOOD);
    setWorldObject(1, RAILS_Y + 1, OBJECT_WOOD);
    setWorldObject(2, RAILS_Y + 1, OBJECT_IRON);
    setWorldObject(3, RAILS_Y + 1, OBJECT_IRON);
    setWorldObject(4, RAILS_Y + 1, OBJECT_AXE);
    setWorldObject(5, RAILS_Y + 1, OBJECT_PICKAXE);

    chunk = -1;

    player.x = TILE_SIZE * 4;
    player.y = TILE_SIZE * (RAILS_Y + 1);
    player.direction = DIR_DOWN;
    player.objectHeld = EMPTY;
    player.quantityHeld = 0;

    if (temp == GAMEMODE_HOST)
    {
        player2.objectHeld = EMPTY;
        player2.quantityHeld = 0;
    }
    else if (temp == GAMEMODE_ASSISTED)
    {
        player2.x = TILE_SIZE * 5;
        player2.y = TILE_SIZE * (RAILS_Y + 1);
        player2.direction = DIR_DOWN;
        player2.objectHeld = EMPTY;
        player2.quantityHeld = 0;
    }

    for (int i = 0; i < WAGONS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            wagons[i]->slots[j] = EMPTY;
            wagons[i]->quantity[j] = 0;
        }
    }

    locomotive.x = 32;
    locomotive.y = (RAILS_Y - 0.2) * TILE_SIZE; // To center on the rails

    // Clean up updates cuz the client will generate by himself
    for (int i = 0; i < MAX_UPDATES; i++)
    {
        updates[i].occupied = false;
    }

    if (temp == GAMEMODE_ASSISTED)
    {
        setBobotSearch(SEARCH_IDLE, EMPTY);
        len = 0;
    }
    gameMode = temp;

    printf("World generation complete\n");
}

bool initLocalWifi()
{
    if (Wifi_CheckInit())
        return true;
    printf("Initializing WiFi...\n");
    if (!Wifi_InitDefault(INIT_ONLY | WIFI_LOCAL_ONLY))
    {
        printf("Can't initialize WiFi!\n");
        delay(2);
        return false;
    }
    printf("WiFi initialized!\n");
    return true;
}

typedef struct
{
    uint8_t has_started;
    int seed;
    uint8_t player_mask;
    bool doneUpdate;
    uint8_t lastReceivedId;
    float locomotiveX;
    float locomotiveSpeed;
    struct Player playerHost;
    struct Player playerClient;
    struct Update update;
} pkt_host_to_client;

void SendHostStateToClients(void)
{
    pkt_host_to_client host_packet;

    host_packet.has_started = gameStarted;
    host_packet.seed = seed;
    host_packet.player_mask = gamePlayerMask;

    host_packet.playerHost.x = player.x;
    host_packet.playerHost.y = player.y;
    host_packet.playerHost.direction = player.direction;
    host_packet.playerHost.animationFrame = player.animationFrame;

    host_packet.locomotiveX = locomotive.x;
    host_packet.locomotiveSpeed = locomotive.speed;

    host_packet.playerClient.objectHeld = player2.objectHeld;
    host_packet.playerClient.quantityHeld = player2.quantityHeld;

    if (doneUpdate)
    {
        host_packet.doneUpdate = true;
        doneUpdate = false;
        host_packet.lastReceivedId = lastReceivedId;
    }
    else
    {
        host_packet.doneUpdate = false;
    }

    host_packet.update.occupied = false;
    for (int i = 0; i < MAX_UPDATES; i++)
    {
        if (updates[i].occupied)
        {
            host_packet.update.occupied = updates[i].occupied;
            host_packet.update.x = updates[i].x;
            host_packet.update.y = updates[i].y;
            host_packet.update.action = updates[i].action;
            host_packet.update.parameter = updates[i].parameter;
            host_packet.update.id = updates[i].id;
            break;
        }
    }

    Wifi_MultiplayerHostCmdTxFrame(&host_packet, sizeof(host_packet));
}

void FromHostPacketHandler(Wifi_MPPacketType type, int base, int len)
{
    if (len < sizeof(pkt_host_to_client))
    {
        // TODO: This shouldn't have happened!
        return;
    }

    if (type != WIFI_MPTYPE_CMD)
        return;

    // Save information received from the client into the global state struct
    pkt_host_to_client packet;
    Wifi_RxRawReadPacket(base, sizeof(packet), (void *)&packet);

    player2.x = packet.playerHost.x;
    player2.y = packet.playerHost.y;
    player2.direction = packet.playerHost.direction;
    player2.animationFrame = packet.playerHost.animationFrame;

    locomotive.x = packet.locomotiveX;
    locomotive.speed = packet.locomotiveSpeed;

    player.objectHeld = packet.playerClient.objectHeld;
    player.quantityHeld = packet.playerClient.quantityHeld;

    if (packet.doneUpdate)
    {
        for (int i = 0; i < MAX_UPDATES; i++)
        {
            if (updates[i].occupied && updates[i].id == packet.lastReceivedId)
            {
                updates[i].occupied = false;
            }
        }
    }

    if (packet.update.occupied && packet.update.id != lastReceivedId && !doneUpdate)
    {
        doneUpdate = true;
        lastReceivedId = packet.update.id;
        switch (packet.update.action)
        {
        case ACTION_SETWORLDTERRAIN:
        {
            int x = packet.update.x;
            int y = packet.update.y;
            int tile = packet.update.parameter;

            worldTerrain[x][y] = tile;
            worldHealth[x][y] = 3;
            worldVariants[x][y] = hash((x << 16) | y, seed) % 4;

            if (x >= (chunk - 5) * 4 && x <= chunk * 4)
            {
                bg0SetTile((x * 2) % 64, y * 2, tile * 4 + worldVariants[x][y] * 16 * 4);
                bg0SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1 + worldVariants[x][y] * 16 * 4);
                bg0SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2 + worldVariants[x][y] * 16 * 4);
                bg0SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3 + worldVariants[x][y] * 16 * 4);
            }
        }
        break;
        case ACTION_SETWORLDOBJECT:
        {
            int x = packet.update.x;
            int y = packet.update.y;
            int tile = packet.update.parameter;

            worldObjects[x][y] = tile;

            if (x >= (chunk - 5) * 4 && x < chunk * 4)
            {
                bg1SetTile((x * 2) % 64, y * 2, tile * 4);
                bg1SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1);
                bg1SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2);
                bg1SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3);
            }
        }
        break;
        case ACTION_SETWORLDHEALTH:
            if (packet.update.parameter > 0)
            {
                int x = packet.update.x;
                int y = packet.update.y;
                int health = packet.update.parameter;

                worldHealth[x][y] = health;
                int tile = worldTerrain[x][y] + (3 - health) * 2;

                if (x >= (chunk - 5) * 4 && x < chunk * 4)
                {
                    bg0SetTile((x * 2) % 64, y * 2, tile * 4 + worldVariants[x][y] * 16 * 4);
                    bg0SetTile((x * 2 + 1) % 64, y * 2, tile * 4 + 1 + worldVariants[x][y] * 16 * 4);
                    bg0SetTile((x * 2) % 64, y * 2 + 1, tile * 4 + 2 + worldVariants[x][y] * 16 * 4);
                    bg0SetTile((x * 2 + 1) % 64, y * 2 + 1, tile * 4 + 3 + worldVariants[x][y] * 16 * 4);
                }
            }
            break;
        case ACTION_SETWAGONOBJECT:
        {
            int wagonId = packet.update.x;
            int slot = packet.update.y;
            int object = packet.update.parameter;

            wagons[wagonId]->slots[slot] = object;
            updateWagon(wagonId);
        }
        break;
        case ACTION_SETWAGONQUANTITY:
        {
            int wagonId = packet.update.x;
            int slot = packet.update.y;
            int quantity = packet.update.parameter;

            wagons[wagonId]->quantity[slot] = quantity;
            updateWagon(wagonId);
        }
        break;
        }
    }

    seed = packet.seed;
    gamePlayerMask = packet.player_mask;
    gameStarted = packet.has_started;
}

typedef struct
{
    u8 x, y, direction, animationFrame;
    bool doneUpdate;
    u8 lastReceivedId;
    struct Update update;
} pkt_client_to_host;

void SendClientStateToHost(void)
{
    pkt_client_to_host packet;
    packet.x = player.x;
    packet.y = player.y;
    packet.direction = player.direction;
    packet.animationFrame = player.animationFrame;

    if (doneUpdate)
    {
        packet.doneUpdate = true;
        doneUpdate = false;
        packet.lastReceivedId = lastReceivedId;
    }
    else
    {
        packet.doneUpdate = false;
    }

    packet.update.occupied = false;
    for (int i = 0; i < MAX_UPDATES; i++)
    {
        if (updates[i].occupied)
        {
            packet.update.occupied = updates[i].occupied;
            packet.update.x = updates[i].x;
            packet.update.y = updates[i].y;
            packet.update.action = updates[i].action;
            packet.update.parameter = updates[i].parameter;
            packet.update.id = updates[i].id;
            break;
        }
        if (i == MAX_UPDATES - 1) // If last update and not occupied, we can proceed
        {
            interact = true;
        }
    }

    Wifi_MultiplayerClientReplyTxFrame(&packet, sizeof(packet));
}

void FromClientPacketHandler(Wifi_MPPacketType type, int aid, int base, int len)
{
    if (len < sizeof(pkt_client_to_host))
    {
        // TODO: This shouldn't have happened!
        return;
    }

    if (type != WIFI_MPTYPE_REPLY)
        return;

    // Save information received from the client into the global state struct
    pkt_client_to_host packet;
    Wifi_RxRawReadPacket(base, sizeof(packet), (void *)&packet);

    player2.x = packet.x;
    player2.y = packet.y;
    player2.direction = packet.direction;
    player2.animationFrame = packet.animationFrame;

    if (packet.doneUpdate)
    {
        for (int i = 0; i < MAX_UPDATES; i++)
        {
            if (updates[i].occupied && updates[i].id == packet.lastReceivedId)
            {
                updates[i].occupied = false;
            }
        }
    }

    if (packet.update.occupied && packet.update.id != lastReceivedId && !doneUpdate)
    {
        doneUpdate = true;
        lastReceivedId = packet.update.id;
        switch (packet.update.action)
        {
        case ACTION_SETWORLDTERRAIN:
            setWorldTile(packet.update.x, packet.update.y, packet.update.parameter);
            break;
        case ACTION_SETWORLDOBJECT:
            setWorldObject(packet.update.x, packet.update.y, packet.update.parameter);
            break;
        case ACTION_SETWORLDHEALTH:
            setWorldHealth(packet.update.x, packet.update.y, packet.update.parameter);
            break;
        case ACTION_SETWAGONOBJECT:
            setWagonObject(packet.update.x, packet.update.y, packet.update.parameter);
            break;
        case ACTION_SETWAGONQUANTITY:
            setWagonQuantity(packet.update.x, packet.update.y, packet.update.parameter);
            break;
        case ACTION_SETPLAYEROBJECTHELD:
        {
            player2.objectHeld = packet.update.parameter;
        }
        break;
        case ACTION_SETPLAYERQUANTITY:
        {
            player2.quantityHeld = packet.update.parameter;
            if (player2.quantityHeld == 0)
                player2.objectHeld = EMPTY;
        }
        break;
        }
    }

    gamePlayerMask |= BIT(aid);
}

void initHostMode()
{
    Wifi_MultiplayerHostMode(MAX_CLIENTS, sizeof(pkt_host_to_client),
                             sizeof(pkt_client_to_host));
    Wifi_MultiplayerFromClientSetPacketHandler(FromClientPacketHandler);

    while (!Wifi_LibraryModeReady())
        swiWaitForVBlank();

    Wifi_SetChannel(6);
    Wifi_MultiplayerAllowNewClients(true);

    Wifi_BeaconStart("Derailed", 0xDE8A11ED);

    swiWaitForVBlank();
    swiWaitForVBlank();
}

bool AccessPointSelectionMenu(void)
{
    Wifi_MultiplayerClientMode(sizeof(pkt_client_to_host));

    while (!Wifi_LibraryModeReady())
        swiWaitForVBlank();

    Wifi_ScanMode();

    int chosen = 0;

    while (1)
    {
        swiWaitForVBlank();

        scanKeys();
        uint16_t keys = keysDown();

        if (keys & KEY_START)
            return false;

        // Get find out how many APs there are in the area
        int count = Wifi_GetNumAP();

        printf("\x1b[2J");

        printf("Number of nearby hosts: %d\n", count);
        printf("\n");

        if (count == 0)
            continue;

        if (keys & KEY_UP)
        {
            mmEffect(SFX_SELECT);
            chosen--;
        }

        if (keys & KEY_DOWN)
        {
            mmEffect(SFX_SELECT);
            chosen++;
        }

        if (chosen < 0)
            chosen = 0;
        if (chosen >= count)
            chosen = count - 1;

        int first = chosen - 1;
        if (first < 0)
            first = 0;

        int last = first + 3;
        if (last >= count)
            last = count - 1;

        for (int i = first; i <= last; i++)
        {
            Wifi_AccessPoint ap;
            Wifi_GetAPData(i, &ap);

            // Note that the libnds console can only print ASCII characters. If
            // the name uses characters outside of that character set, printf()
            // won't be able to print them.
            char name[10 * 4 + 1];
            int ret = utf16_to_utf8(name, sizeof(name), ap.nintendo.name,
                                    ap.nintendo.name_len * 2);
            if (ret <= 0)
                name[0] = '\0';

            // In multiplayer client mode DSWiFi ignores all access points that
            // don't offer any Nintendo information. Also, DSWiFi host access
            // points don't use any encryption.

            printf("%s %.19s\n", i == chosen ? "> " : "  ", name);
            printf("   Players %d/%d\n", ap.nintendo.players_current,
                   ap.nintendo.players_max);

            if (ap.nintendo.allows_connections)
                printf("    OPEN\n");
            else
                printf("    CLOSED\n");
            printf("\n");

            if (i == chosen)
                AccessPoint = ap;
        }

        if (keys & KEY_A)
        {
            mmEffect(SFX_SELECTED);
            return true;
        }
    }

    printf("\x1b[2J");
}

bool initClientMode()
{
    Wifi_MultiplayerFromHostSetPacketHandler(FromHostPacketHandler);

    printf("Selected network:\n");
    printf("\n");
    printf("%.31s\n", AccessPoint.ssid);
    printf("Channel: %d\n", AccessPoint.channel);
    printf("\n");

    Wifi_ConnectOpenAP(&AccessPoint);

    printf("\x1b[2J");

    printf("Connecting to AP\n");
    printf("Press START to cancel\n");
    printf("\n");

    // Wait until we're connected

    int oldstatus = -1;
    while (1)
    {
        swiWaitForVBlank();

        scanKeys();
        if (keysDown() & KEY_START)
            return false;

        int status = Wifi_AssocStatus();

        if (status != oldstatus)
        {
            printf("%s\n", ASSOCSTATUS_STRINGS[status]);
            oldstatus = status;
        }

        if (status == ASSOCSTATUS_CANNOTCONNECT)
        {
            printf("\n");
            printf("Cannot connect to AP\n");
            printf("Press START to go back to reset.\n");

            while (1)
            {
                swiWaitForVBlank();
                scanKeys();
                if (keysDown() & KEY_START)
                    return false;
            }
        }

        if (status == ASSOCSTATUS_ASSOCIATED)
            break;
    }

    printf("Connected to host!\n");
    return true;
}

bool isObject(int x, int y, int object)
{
    return worldObjects[x][y] == object;
}

#define MAINMENU_CHOICES 7

int main(int argc, char **argv)
{
    srand(time(NULL));
    consoleDemoInit();
    if (!nitroFSInit(NULL))
    {
        perror("nitroFSInit()");
        while (1)
        {
            swiWaitForVBlank();
            uint16_t keys = keysDown();
            if (keys & KEY_START)
                return 0;
        }
    }
    if (!mmInitDefault("nitro:/soundbank.bin"))
    {
        perror("mmInitDefault()");
        while (1)
        {
            swiWaitForVBlank();
            uint16_t keys = keysDown();
            if (keys & KEY_START)
                return 0;
        }
    }

    mmLoad(MOD_JOINT_PEOPLE);
    mmLoadEffect(SFX_SELECT);
    mmLoadEffect(SFX_SELECTED);
    mmLoadEffect(SFX_EXPLOSION);
    mmLoadEffect(SFX_BREAK);
    mmLoadEffect(SFX_WIN);
    mmLoadEffect(SFX_PICKUP);

start:

    mmStart(MOD_JOINT_PEOPLE, MM_PLAY_LOOP);

    if (Wifi_CheckInit())
    {
        if (Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED)
            Wifi_DisconnectAP();
        Wifi_Deinit();
    }
    gameMode = GAMEMODE_ASSISTED;
    gameStarted = 0;
    gamePlayerMask = 0;

    lastId = 0;
    doneUpdate = false;
    lastReceivedId = 255;

    worldPart = 0;
    locomotive.speed = 0.01f;

    videoSetMode(MODE_0_2D);

    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_SPRITE, VRAM_C_LCD, VRAM_D_LCD);

    consoleDemoInit();
    // white bg with red text
    BG_PALETTE_SUB[0] = RGB15(31, 31, 31);
    consoleSetColor(NULL, CONSOLE_RED);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);

    dmaCopy(introTiles, bgGetGfxPtr(bg0), introTilesLen);
    dmaCopy(introMap, bgGetMapPtr(bg0), introMapLen);
    dmaCopy(introPal, BG_PALETTE, introPalLen);

    int selection = 0;
    while (1)
    {
        swiWaitForVBlank();
        scanKeys();

        printf("\x1b[2J");
        for (int i = 0; i < MAINMENU_CHOICES; i++)
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
            case 2:
                printf("Host Game\n");
                break;
            case 3:
                printf("Join Game\n");
                break;
            case 4:
                printf("Credits\n");
                break;
            case 5:
                printf("Exit Game\n");
                break;
            case 6:
                printf("Assisted mode (robot): %s\n", gameMode == GAMEMODE_ASSISTED ? "ON" : "OFF");
                break;
            default:
                printf("???\n");
                break;
            }
        }

        if (keysDown() & KEY_UP)
        {
            selection--;
            if (selection < 0)
                selection = MAINMENU_CHOICES - 1;
            mmEffect(SFX_SELECT);
        }
        if (keysDown() & KEY_DOWN)
        {
            selection++;
            if (selection > MAINMENU_CHOICES - 1)
                selection = 0;
            mmEffect(SFX_SELECT);
        }
        if (keysDown() & KEY_A)
        {
            mmEffect(SFX_SELECTED);
            if (selection == 0) // Choose Seed
            {
                printf("\x1b[2J");
                printf("Enter Seed:\nUse arrow keys to change digits, A to confirm\n");
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
                        mmEffect(SFX_SELECT);
                    }
                    else if (keysDown() & KEY_RIGHT)
                    {
                        selection++;
                        if (selection > 7)
                            selection = 0;
                        mmEffect(SFX_SELECT);
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

                        mmEffect(SFX_SELECT);
                    }
                    else if (keysDown() & KEY_A)
                    {
                        mmEffect(SFX_SELECTED);
                        goto generate;
                    }
                }
            }
            else if (selection == 1) // Random Seed
            {
                seed = rand() % 0xFFFFFFFF;
                goto generate;
            }
            else if (selection == 2) // Host Game
            {
                if (!initLocalWifi())
                    goto start;

                initHostMode();

                printf("Host ready!\n");

                while (1)
                {
                    swiWaitForVBlank();

                    scanKeys();

                    u16 keys_down = keysDown();

                    printf("\x1b[2J");
                    if (Wifi_MultiplayerGetNumClients() > 0)
                        printf("Player connected!\nPress A to start the game\n");
                    else
                        printf("Waiting for another player...\n");
                    printf("\n");

                    if ((keys_down & KEY_A) && Wifi_MultiplayerGetNumClients() > 0)
                    {
                        mmEffect(SFX_SELECTED);
                        break;
                    }
                    if ((keys_down & KEY_START))
                        goto start;

                    int num_clients = Wifi_MultiplayerGetNumClients();

                    // Print all client information. This normally isn't needed, all you
                    // need is the mask of AIDs.
                    Wifi_ConnectedClient client[15];
                    num_clients = Wifi_MultiplayerGetClients(15, &(client[0]));

                    for (int i = 0; i < num_clients; i++)
                    {
                        printf("Connected player: %04X:%04X:%04X\n", client[i].macaddr[0], client[i].macaddr[1],
                               client[i].macaddr[2]);
                    }
                }

                Wifi_MultiplayerAllowNewClients(false);
                gameMode = GAMEMODE_HOST;
                gameStarted = 1;

                printf("Game starting...\n");
                delay(1);

                seed = rand() % 0xFFFFFFFF;
                goto generate;
            }
            else if (selection == 3) // Join Game
            {
                if (!initLocalWifi())
                    goto start;

                if (!AccessPointSelectionMenu())
                {
                    goto start;
                }

                if (!initClientMode())
                {
                    goto start;
                }

                printf("\x1b[2J");

                printf("Connected successfully!\n\n");

                printf("Waiting for the host to start the game...\n\n");

                printf("START: Reset\n");

                while (gameStarted == 0)
                {
                    swiWaitForVBlank();

                    scanKeys();

                    u16 keys_down = keysDown();
                    if (keys_down & KEY_START)
                    {
                        goto start;
                    }
                }
                gameMode = GAMEMODE_CLIENT;

                printf("Game started!\n");
                delay(1);

                goto generate;
            }
            else if (selection == 4) // Credits
            {
                printf("\x1b[2J");
                printf("Derailed!\n\
Version: %s\n\
https://github.com/AzizBgBoss/derailed/\n\
Commit %s\n\
\n\
Made by: AzizBgBoss\n\
\n\
Special thanks to:\n\
AntonioND for making the BlocksDS toolchain\n\
Anadune & Floppy for the music\n\
\n\
\n\
And you for trying Derailed! out\n\
\n\
Try other DS games at https://github.com/AzizBgBoss/\n\
\n\
Press START to go back.\n",
                       VERSION,
                       COMMIT_HASH);
                while (1)
                {
                    swiWaitForVBlank();

                    scanKeys();

                    u16 keys_down = keysDown();
                    if (keys_down & KEY_START)
                    {
                        goto start;
                    }
                }
            }
            else if (selection == 6) // Assisted mode
            {
                gameMode = gameMode == GAMEMODE_ASSISTED ? GAMEMODE_SINGLEPLAYER : GAMEMODE_ASSISTED;
            }
            else // Exit Game
                return 0;
        }
    }

generate:

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
    player.gfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(playerTiles, player.gfx, 8 * 8 * 4); // Tile size X * Y * 4 tiles * 2 bytes (u16)
    u16 *cursorGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(uiTiles + 8 * 8 * 4, cursorGfx, 8 * 8 * 4);
    locomotive.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles, locomotive.gfx, 8 * 8 * 4 * 2);
    railStorage.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles + 8 * 8 * 2, railStorage.gfx, 8 * 8 * 4 * 2);
    railBuilder.gfx = oamAllocateGfx(&oamMain, SpriteSize_32x16, SpriteColorFormat_256Color);
    dmaCopy(wagonsTiles + 8 * 8 * 2 * 2, railBuilder.gfx, 8 * 8 * 4 * 2);
    player2.gfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    if (gameMode == GAMEMODE_ASSISTED)
        dmaCopy(robotTiles, player2.gfx, 8 * 8 * 4);
    else
        dmaCopy(player2Tiles, player2.gfx, 8 * 8 * 4); // Tile size X * Y * 4 tiles * 2 bytes (u16)

    // Copy palette
    dmaCopy(playerPal, SPRITE_PALETTE, playerPalLen);

    oamSet(&oamMain, 0,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_16x16, SpriteColorFormat_256Color, // Size, format
           player.gfx,                                   // Graphics offset
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

    oamSet(&oamMain, 5,
           0, 0,                                         // X, Y
           0,                                            // Priority
           0,                                            // Palette index
           SpriteSize_16x16, SpriteColorFormat_256Color, // Size, format
           player2.gfx,                                  // Graphics offset
           -1,                                           // Affine index
           false,                                        // Double size
           false,                                        // Hide
           false, false,                                 // H flip, V flip
           false);                                       // Mosaic

    mmStop();
    generateWorld(seed);
    mmStart(MOD_JOINT_PEOPLE, MM_PLAY_LOOP);

    interact = true;

    while (1)
    {
        swiWaitForVBlank();

        if (gameMode == GAMEMODE_HOST)
        {
            if (Wifi_MultiplayerGetNumClients() == 0)
            {
                printf("\x1b[2J");
                printf("All clients disconnected! Returning to main menu...\n");
                delay(2);
                goto start;
            }
            SendHostStateToClients();
        }
        else if (gameMode == GAMEMODE_CLIENT)
        {
            SendClientStateToHost();
        }

        scanKeys();

        int held = keysHeld();
        int down = keysDown();
        if (down & KEY_START)
            goto start;
        if (down & KEY_SELECT)
            debugMode = !debugMode;

        if (gameMode == GAMEMODE_ASSISTED && searchStep == SEARCH_IDLE)
        {
            if (down & KEY_L)
            {
                int x;
                int y;
                if (find_closest_object(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), OBJECT_AXE, isObject, &x, &y))
                {
                    setBobotSearch(SEARCH_SEARCHING, OBJECT_AXE);
                    len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), x, y, path, sizeof(path) / sizeof(int));
                }
                else
                {
                    setBobotMessage(SEARCH_BLOCKED, searchObject);
                }
            }
            if (down & KEY_R)
            {
                int x;
                int y;
                if (find_closest_object(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), OBJECT_PICKAXE, isObject, &x, &y))
                {
                    setBobotSearch(SEARCH_SEARCHING, OBJECT_PICKAXE);
                    len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), x, y, path, sizeof(path) / sizeof(int));
                }
                else
                {
                    setBobotMessage(SEARCH_BLOCKED, searchObject);
                }
            }
            if (down & KEY_Y)
            {
                int x;
                int y;
                if (find_closest_object(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), OBJECT_WOOD, isObject, &x, &y))
                {
                    setBobotSearch(SEARCH_SEARCHING, OBJECT_WOOD);
                    len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), x, y, path, sizeof(path) / sizeof(int));
                }
                else
                {
                    setBobotMessage(SEARCH_BLOCKED, searchObject);
                }
            }
            if (down & KEY_X)
            {
                int x;
                int y;
                if (find_closest_object(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), OBJECT_IRON, isObject, &x, &y))
                {
                    setBobotSearch(SEARCH_SEARCHING, OBJECT_IRON);
                    len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), x, y, path, sizeof(path) / sizeof(int));
                }
                else
                {
                    setBobotMessage(SEARCH_BLOCKED, searchObject);
                }
            }
        }

        int newX = player.x;
        int newY = player.y;

        if (frames % 10 == 0)
            player.animationFrame = (player.animationFrame + 1) % 4;

        if (held & KEY_UP)
        {
            newY--;
            player.direction = DIR_UP;
        }
        else if (held & KEY_DOWN)
        {
            newY++;
            player.direction = DIR_DOWN;
        }

        if (held & KEY_LEFT)
        {
            newX--;
            player.direction = DIR_LEFT;
        }
        else if (held & KEY_RIGHT)
        {
            newX++;
            player.direction = DIR_RIGHT;
        }

        if (!(held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)))
        {
            player.animationFrame = 0;
            dmaCopy(((gameMode == GAMEMODE_CLIENT) ? player2Tiles : playerTiles) + 8 * 8 * 4 * player.direction, player.gfx, 8 * 8 * 4);
        }
        else
            dmaCopy(((gameMode == GAMEMODE_CLIENT) ? player2Tiles : playerTiles) + 8 * 8 * 4 * player.direction + 8 * 8 * player.animationFrame, player.gfx, 8 * 8 * 4);

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
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_DOWN)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE + 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] - 1);
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_LEFT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE - 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] - 1);
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_RIGHT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] == TILE_TREE)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE + 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] - 1);
                        mmEffect(SFX_BREAK);
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
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_DOWN)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE, ((int)player.y + 8) / TILE_SIZE + 1, worldHealth[((int)player.x + 8) / TILE_SIZE][((int)player.y + 8) / TILE_SIZE + 1] - 1);
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_LEFT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE - 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE - 1][((int)player.y + 8) / TILE_SIZE] - 1);
                        mmEffect(SFX_BREAK);
                    }
                }
                else if (player.direction == DIR_RIGHT)
                {
                    if (worldTerrain[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] == TILE_ROCK)
                    {
                        setWorldHealth(((int)player.x + 8) / TILE_SIZE + 1, ((int)player.y + 8) / TILE_SIZE, worldHealth[((int)player.x + 8) / TILE_SIZE + 1][((int)player.y + 8) / TILE_SIZE] - 1);
                        mmEffect(SFX_BREAK);
                    }
                }
            }
        }

        if (frames % (60 * 3) == 0 && gameMode != GAMEMODE_CLIENT) // Wassup baby girl? Let me handle it (said the host to the client in an attractive way)
        {
            if (railStorage.quantity[0] && railStorage.quantity[1] && railBuilder.quantity[0] < railBuilder.maxQuantity)
            {
                // Take one wood and one iron from storage and turn them into a rail through the builder
                setWagonObject(2, 0, OBJECT_RAIL);
                setWagonQuantity(2, 0, railBuilder.quantity[0] + 1);

                setWagonQuantity(1, 0, railStorage.quantity[0] - 1);
                setWagonQuantity(1, 1, railStorage.quantity[1] - 1);
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
        if (interact &&
            player.objectHeld == worldObjects[player.selectedObjectX][player.selectedObjectY] &&
            player.quantityHeld < player.maxQuantityHeld &&
            justPlaced == false &&
            worldObjects[player.selectedObjectX][player.selectedObjectY] != EMPTY &&
            worldObjects[player.selectedObjectX][player.selectedObjectY] != OBJECT_RAIL) // Don't include rails for now to avoid unintentional pick up in the main railway
        {
            setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
            setPlayerQuantity(player.quantityHeld + 1);
            mmEffect(SFX_PICKUP);
            if (gameMode == GAMEMODE_CLIENT)
                interact = false;
        }

        if (worldObjects[player.selectedObjectX][player.selectedObjectY] != EMPTY && !player.selectedWagon)
        {
            if (worldObjects[player.selectedObjectX][player.selectedObjectY] == OBJECT_RAIL)
            {
                //   x
                // x R x      Check these tiles and make sure the rail isnt intermediate between two other rails
                //   x        if it is, then it is not selected
                if (((player.selectedObjectX >= locomotive.x / TILE_SIZE &&
                     player.selectedObjectX <= (locomotive.x + locomotive.sizeX) / TILE_SIZE) ||

                    (player.selectedObjectX == 0 &&
                     worldObjects[player.selectedObjectX + 1][player.selectedObjectY] == OBJECT_RAIL) ||

                    (worldObjects[player.selectedObjectX - 1][player.selectedObjectY] == OBJECT_RAIL &&
                     worldObjects[player.selectedObjectX + 1][player.selectedObjectY] == OBJECT_RAIL)) &&
                    player.selectedObjectY == RAILS_Y)

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

        if (down & KEY_A && interact)
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
                        setPlayerObjectHeld(temp);
                        mmEffect(SFX_PICKUP);
                        if (gameMode == GAMEMODE_CLIENT)
                            interact = false;
                    }
                    else if (worldObjects[player.selectedObjectX][player.selectedObjectY] == player.objectHeld &&
                             player.quantityHeld < player.maxQuantityHeld)
                    {
                        // Stack object
                        setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
                        setPlayerQuantity(player.quantityHeld + 1);
                        mmEffect(SFX_PICKUP);
                        if (gameMode == GAMEMODE_CLIENT)
                            interact = false;
                    }
                }
                else if (player.selectedWagon)
                {
                    // Put object in wagon
                    setWagonObject(player.selectedWagonId, player.selectedWagonSlot, player.objectHeld);
                    int temp1 = player.quantityHeld;
                    int temp2 = wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot];
                    while (temp1 > 0 && temp2 < wagons[player.selectedWagonId]->maxQuantity)
                    {
                        temp1--;
                        temp2++;
                    }

                    setWagonQuantity(player.selectedWagonId, player.selectedWagonSlot, temp2);
                    setPlayerQuantity(temp1);

                    updateWagon(player.selectedWagonId);
                    mmEffect(SFX_PICKUP);
                    if (gameMode == GAMEMODE_CLIENT)
                        interact = false;
                }
                else if (worldObjects[player.selectedObjectX][player.selectedObjectY] == EMPTY)
                {
                    // Place object
                    setWorldObject(player.selectedObjectX, player.selectedObjectY, player.objectHeld);
                    setPlayerQuantity(player.quantityHeld - 1);
                    lastPlacedX = player.selectedObjectX;
                    lastPlacedY = player.selectedObjectY;
                    justPlaced = true;
                    mmEffect(SFX_PICKUP);
                    if (gameMode == GAMEMODE_CLIENT)
                        interact = false;
                }
            }
            else if (player.selectedWagon) // Playing is selecting an output wagon
            {
                setPlayerObjectHeld(wagons[player.selectedWagonId]->slots[player.selectedWagonSlot]);
                int temp1 = wagons[player.selectedWagonId]->quantity[player.selectedWagonSlot];
                int temp2 = player.quantityHeld;
                while (temp1 > 0 && temp2 < player.maxQuantityHeld)
                {
                    temp1--;
                    temp2++;
                }

                setWagonQuantity(player.selectedWagonId, player.selectedWagonSlot, temp1);
                setPlayerQuantity(temp2);

                mmEffect(SFX_PICKUP);
                if (gameMode == GAMEMODE_CLIENT)
                    interact = false;
            }
            else if (player.selectedObject)
            {
                // Pick up object
                setPlayerObjectHeld(worldObjects[player.selectedObjectX][player.selectedObjectY]);
                setWorldObject(player.selectedObjectX, player.selectedObjectY, EMPTY);
                setPlayerQuantity(1);
                justPlaced = true;
                lastPlacedX = player.selectedObjectX;
                lastPlacedY = player.selectedObjectY;
                mmEffect(SFX_PICKUP);
                if (gameMode == GAMEMODE_CLIENT)
                    interact = false;
            }
        }

        // test for locomotive derailment
        if (locomotive.direction == DIR_RIGHT)
        {
            if (locomotive.x + locomotive.sizeX + locomotive.speed >= WORLD_WIDTH * TILE_SIZE)
            {
                locomotive.speed *= 1.5f;
                printf("\x1b[2J");
                printf("You won :)\n\nThe train's speed will now be %.3fm/s", (locomotive.speed * 60) / TILE_SIZE);
                worldPart++;
                mmStop();
                mmEffect(SFX_WIN);
                delay(3);
                goto generate;
            }
            else if (worldObjects[(int)(locomotive.x + locomotive.sizeX + locomotive.speed) / TILE_SIZE][(int)(locomotive.y + locomotive.sizeY) / TILE_SIZE] != OBJECT_RAIL)
            {
                printf("\x1b[2J");
                printf("You lost :(\n");
                mmStop();
                mmEffect(SFX_EXPLOSION);
                delay(2);
                goto start;
            }
            else
            {
                if (gameMode != GAMEMODE_CLIENT)
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

        // Robot intelligence
        if (gameMode == GAMEMODE_ASSISTED && frames % 2 == 0)
        {
            // Pathfinding, shit
            int targetX = 0;
            int targetY = 0;

            if (len > 0)
            {
                if (target == -1)
                {
                    target = 0;
                }

                if (player2.x == TILE_TO_PX(pf_node_x(path[target])) && player2.y == TILE_TO_PX(pf_node_y(path[target])))
                {
                    target++;
                }

                if (target >= len)
                {
                    target = -1;
                    len = 0;
                }

                targetX = TILE_TO_PX(pf_node_x(path[target]));
                targetY = TILE_TO_PX(pf_node_y(path[target]));
            }
            else
                target = -1;

            int new2X = player2.x;
            int new2Y = player2.y;

            if (target != -1)
            {
                if (player2.x > targetX)
                {
                    new2X--;
                    player2.direction = DIR_LEFT;
                }
                else if (player2.x < targetX)
                {
                    new2X++;
                    player2.direction = DIR_RIGHT;
                }
                if (player2.y > targetY)
                {
                    new2Y--;
                    player2.direction = DIR_UP;
                }
                else if (player2.y < targetY)
                {
                    new2Y++;
                    player2.direction = DIR_DOWN;
                }
            }

            if (new2X == player2.x && new2Y == player2.y)
            {
                player2.animationFrame = 0;
                dmaCopy(robotTiles + 8 * 8 * 4 * player2.direction, player2.gfx, 8 * 8 * 4);
            }
            else
            {
                if (frames % 10 == 0)
                    player2.animationFrame = (player2.animationFrame + 1) % 4;
                dmaCopy(robotTiles + 8 * 8 * 4 * player2.direction + 8 * 8 * player2.animationFrame, player2.gfx, 8 * 8 * 4);
            }

            // if (!checkCollision(new2X, player2.y))
            {
                player2.x = new2X;
            }
            // if (!checkCollision(player2.x, new2Y))
            {
                player2.y = new2Y;
            }
        }

        if (gameMode == GAMEMODE_ASSISTED && frames % 60 == 0)
        {
            if (searchStep == SEARCH_SEARCHING)
            {
                if (len == 0) // Found trouble, retry
                {
                    int x;
                    int y;
                    if (find_closest_object(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), searchObject, isObject, &x, &y))
                    {
                        len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), x, y, path, sizeof(path) / sizeof(int));
                    }
                    else
                    {
                        setBobotSearch(SEARCH_IDLE, searchObject);
                    }
                }
                if (worldObjects[PX_TO_TILE((int)player2.x)][PX_TO_TILE((int)player2.y)] == searchObject)
                {
                    setWorldObject(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), EMPTY);
                    setBobotSearch(SEARCH_RETURNING, searchObject);
                }
            }
            else if (searchStep == SEARCH_RETURNING) // Found target, go back to player
            {
                if (len == 0)
                {
                    len = pf_find_path(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), PX_TO_TILE((int)player.x + TILE_SIZE / 2), PX_TO_TILE((int)player.y + TILE_SIZE / 2), path, sizeof(path) / sizeof(int));
                }
                if (PX_TO_TILE((int)player2.x) == PX_TO_TILE((int)player.x + TILE_SIZE / 2) && PX_TO_TILE((int)player2.y) == PX_TO_TILE((int)player.y + TILE_SIZE / 2))
                {
                    if (worldObjects[PX_TO_TILE((int)player2.x)][PX_TO_TILE((int)player2.y)] == EMPTY)
                    {
                        setWorldObject(PX_TO_TILE((int)player2.x), PX_TO_TILE((int)player2.y), searchObject);
                        setBobotSearch(SEARCH_IDLE, searchObject);
                    }
                }
            }
        }

        if (locomotive.x + locomotive.sizeX >= WORLD_WIDTH * TILE_SIZE - SCREEN_WIDTH + 32 * 3)
            scroll = WORLD_WIDTH * TILE_SIZE - SCREEN_WIDTH;
        else if (locomotive.x + locomotive.sizeX >= 32 * 3)
            scroll = locomotive.x + locomotive.sizeX - 32 * 3;
        else
            scroll = 0;

        // Chunking (btw, player never goes back to left)
        if ((scroll + SCREEN_WIDTH) / TILE_SIZE >= chunk * 4)
        {
            chunk++;
            if (chunk < WORLD_WIDTH / 4)
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

        if (gameMode != GAMEMODE_SINGLEPLAYER)
        {
            if (gamePlayerMask & BIT(1) || gameMode == GAMEMODE_ASSISTED)
            {
                if (player2.x >= scroll - TILE_SIZE && player2.x < scroll + SCREEN_WIDTH)
                {
                    oamSetXY(&oamMain, 5, player2.x - scroll, player2.y);

                    if (gameMode == GAMEMODE_HOST)
                        dmaCopy(player2Tiles + 8 * 8 * 4 * player2.direction + 8 * 8 * player2.animationFrame, player2.gfx, 8 * 8 * 4);
                    else if (gameMode == GAMEMODE_CLIENT)
                        dmaCopy(playerTiles + 8 * 8 * 4 * player2.direction + 8 * 8 * player2.animationFrame, player2.gfx, 8 * 8 * 4);
                    else if (gameMode == GAMEMODE_ASSISTED)
                        dmaCopy(robotTiles + 8 * 8 * 4 * player2.direction + 8 * 8 * player2.animationFrame, player2.gfx, 8 * 8 * 4);
                }
                else
                    oamSetXY(&oamMain, 5, -16, -16);
            }
            else
                oamSetXY(&oamMain, 5, -16, -16);
        }
        else
            oamSetXY(&oamMain, 5, -16, -16);

        bgSetScroll(bg0, scroll, 0);
        bgSetScroll(bg1, scroll, 0);

        bgUpdate();
        oamUpdate(&oamMain);

        printf("\x1b[2J");

        printf("Progress: %.1f%%\n", locomotive.x * 100 / (WORLD_WIDTH * TILE_SIZE - locomotive.sizeX));
        printf("Train speed: %.3fm/s\n", (locomotive.speed * 60) / TILE_SIZE);
        printf("Seed: %x, part: %d\n\n", seed, worldPart + 1);

        if (!debugMode)
        {
            if (gameMode == GAMEMODE_ASSISTED)
            {
                consoleSetColor(NULL, CONSOLE_GRAY);
                printf("Bobot: %s\n\n", bobotMessage);
            }
            if (player.objectHeld != EMPTY)
            {
                consoleSetColor(NULL, CONSOLE_LIGHT_BLUE);
                if (player.objectHeld != OBJECT_AXE && player.objectHeld != OBJECT_PICKAXE)
                    printf("Holding: %s x %d\n\n", getObjectName(player.objectHeld), player.quantityHeld);
                else
                    printf("Holding: %s\n\n", getObjectName(player.objectHeld));

                consoleSetColor(NULL, CONSOLE_BLUE);
                printf("%s\n", getObjectDescription(player.objectHeld));
            }

            consoleSetColor(NULL, CONSOLE_BLACK);
            if (gameMode == GAMEMODE_HOST)
                printf("\x1b[23;0HHost Mode");
            else if (gameMode == GAMEMODE_CLIENT)
                printf("\x1b[23;0HClient Mode");
            else if (gameMode == GAMEMODE_ASSISTED)
                printf("\x1b[23;0HAssisted Mode");
        }
        else
        {
            printf("x: %f, y: %f, dir: %d, obj: %d\n", player.x, player.y, player.direction, player.selectedObject);
            printf("x: %d, y: %d, object: %d\n", player.selectedObjectX, player.selectedObjectY, worldObjects[player.selectedObjectX][player.selectedObjectY]);
            printf("obj held: %d, quantity: %d\n", player.objectHeld, player.quantityHeld);
            printf("chunk: %d, scroll: %d\n", chunk, scroll);
            printf("interact: %d\n", interact);

            if (gamePlayerMask & BIT(1))
                if (gameMode != GAMEMODE_SINGLEPLAYER)
                {
                    printf("Player %d - x: %f, y: %f\n", 2, player2.x, player2.y);
                    printf("lastId: %d, doneUpdate: %d, lastRId: %d\n", lastId, doneUpdate, lastReceivedId);
                    printf("Updates:\n");
                    for (int i = 0; i < MAX_UPDATES; i++)
                    {
                        printf("\x1b[15;%dH%d", i * 2, updates[i].occupied);
                        printf("\x1b[16;%dH%x", i * 2, updates[i].id);
                    }
                    printf("\n");
                }

            if (gameMode == GAMEMODE_HOST)
            {
                int num_clients = Wifi_MultiplayerGetNumClients();
                printf("Num clients: %d (mask 0x%02X)\n", num_clients, gamePlayerMask);
                printf("\n");
            }

            printf("Press SELECT to disable debug mode\n");
        }

        consoleSetColor(NULL, CONSOLE_RED);

        frames++;
    }

    return 0;
}
