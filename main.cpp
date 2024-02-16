#include <Windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <cmath>
#define M_PI    3.14159265358979323846
#define WIN32_LEAN_AND_MEAN

enum STATUS {
    STOP = 0,
    RUN
};

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

float distanceVector3(Vector3 &a, Vector3 &b) {
    return sqrtf(powf(b.x - a.x, 2) + powf(b.y - a.y, 2) + powf(b.z - a.z, 2));
}

float radianToDegree(float radian) {
    return (radian * 180.0f / M_PI);
}

void aimTo(Vector2 &camera, Vector3& pawn, Vector3& target) {

    Vector3 distance;
    distance.x = target.x - pawn.x; // Profondeur
    distance.y = target.y - pawn.y; // Profondeur 
    distance.z = target.z - pawn.z; // Hauteur 
    Vector2 top;

    // Horizontal
    top.x = atan2f(distance.y, distance.x);
    top.x = radianToDegree(top.x) + 180.0f;
    if (top.x > 180.0f) {
        top.x = (-360.0f + top.x);
    }
    else if (top.x < -180.0f) {
        top.x = (360.0f - top.x);
    }
    camera.y = top.x;

    // Vertical
    top.y = atan2f(distance.z, sqrtf(powf(distance.x, 2.0f) + powf(distance.y, 2.0f)));
    top.y = radianToDegree(top.y);
    if (top.y > 89.0f) {
        top.y = 89.0f;
    }
    else if (top.y < -89.0f) {
        top.y = -89.0f;
    }
    camera.x = top.y;
}

typedef struct LocalCamera
{
    char pad_0000[1416]; //0x0000
    Vector2 angle; //0x0588
    char pad_0590[2736]; //0x0590
}; //Size: 0x1040

typedef struct Entity
{
    uint32_t nothing; //0x0000
    Vector3 bodypos; //0x0004
    Vector3 headpos; //0x0010
    char pad_001C[164]; //0x001C
    float health; //0x00C0
    char pad_00C4[20]; //0x00C4
    char wpAmmo[4][8]; //0x00D8
    char pad_00F8[32]; //0x00F8
    Vector2 camera; //0x0118
    char pad_0120[40]; //0x0120
    float armor; //0x0148
    char pad_014C[432]; //0x014C
    Vector3 pos3d; //0x02FC
} Entity;

namespace Global {
    uint64_t moduleBaseAddress = 0x0;
    Entity** entitylistPTR = nullptr;
    Entity *localplayerPTR = nullptr;
    uint64_t entitylistAddress = 0x0;
    Vector2 screen = { 1280.0f, 720.0f };
    Vector2 screenDiv = { screen.x/2.0f, screen.y/2.0f };
    float *viewMatrix; // [16]
    LocalCamera *localCamera;
    int nbrSlotsServer = 0;
    bool status = STOP;
    namespace Settings {
        bool rageBot = true;
    }
};

namespace Offset {
    const uint64_t  entitylist = 0x01284D80;    //  Address where start entitylist, is just array of pointer
    const unsigned int  beginEntityList = 0x18; //  First pointer in entitylist, so [0x01284D80 + 0x18]; is localplayer
    const unsigned int nextPlayer = 0x10;       //  Size of this type to go next player in entitylist;
    const   uint64_t nbrSlotsServer = 0x7CFD0C;
    const   uint64_t localCamera = 0x00627810;
    // const   uint64_t viewMatrix = 0x1197088;
};


int UPDATE_GLOBAL();
void RANDOMNAME() {
    while (Global::status == RUN)
    {
        if (UPDATE_GLOBAL() == -1) {
            continue;
        }
        if (GetAsyncKeyState(VK_END) & 0x1) {
            Global::status = STOP;
        }
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8001) {
            Entity* localplayer = Global::localplayerPTR;
            if (localplayer == nullptr) continue;
            Entity* currentPlayer = nullptr;
            if (Global::Settings::rageBot) {
                float closeDist = 100000.0f;
                Entity* closePlayer = *(Entity**)((BYTE*)Global::entitylistPTR + 1 * Offset::nextPlayer);
                if (closePlayer == nullptr) continue;
                for (int i = 0; i < Global::nbrSlotsServer; i++) {
                    currentPlayer = *(Entity**)((BYTE*)Global::entitylistPTR + i * Offset::nextPlayer);
                    if (currentPlayer == nullptr) continue;
                    if (currentPlayer != localplayer) {
                        if (currentPlayer->health <= 0.0f) continue;
                        // printf("Player[%d]\nLife: %f\n", i, currentPlayer->health);
                        const float tmpDist = abs(distanceVector3(currentPlayer->headpos, localplayer->headpos));
                        if (tmpDist < closeDist) {
                            closePlayer = currentPlayer;
                            closeDist = tmpDist;
                        }
                    }
                }
                if (closePlayer)
                    aimTo(Global::localCamera->angle, closePlayer->headpos, localplayer->headpos);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void INIT_GLOBAL() {
    Global::moduleBaseAddress = reinterpret_cast<uint64_t>(GetModuleHandle("xonotic.exe"));
    Global::status = RUN;
}

int UPDATE_GLOBAL() {
    if ((unsigned int*)(Global::moduleBaseAddress + Offset::nbrSlotsServer)) {
        Global::nbrSlotsServer = *(unsigned int*)(Global::moduleBaseAddress + Offset::nbrSlotsServer);
        if (Global::nbrSlotsServer <= 0) return -1;
    }
    if ((uint64_t*)(Global::moduleBaseAddress + Offset::entitylist)) {
        Global::entitylistAddress = *(uint64_t*)(Global::moduleBaseAddress + Offset::entitylist);
        if (Global::entitylistAddress == NULL) return -1;
        if ((Entity**)(Global::entitylistAddress + Offset::beginEntityList) == nullptr) return -1;
        Global::entitylistPTR = (Entity**)(Global::entitylistAddress + Offset::beginEntityList);
        Global::localplayerPTR = *Global::entitylistPTR;
        if (Global::localplayerPTR == nullptr) return -1;
    }
    else {
        return -1;
    }
    if ((uint64_t**)(Global::moduleBaseAddress + Offset::localCamera)) {
        Global::localCamera = (LocalCamera*)*(uint64_t**)(Global::moduleBaseAddress + Offset::localCamera);
        if (Global::localCamera == nullptr) return -1;
    }
    return 0;
}

DWORD WINAPI threadMain(HMODULE hModule)
{
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    INIT_GLOBAL();
    std::cout << "[+]   Dll injected successfully" << std::endl;
    std::cout << "      Press END to eject the Dll" << std::endl;
    if (Global::moduleBaseAddress != 0x0) {
        RANDOMNAME();
    }
    std::cout << "[+]   Dll injected ejected" << std::endl;
    if (fp)
        fclose(fp);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
};

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        const HANDLE thread = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(threadMain), hModule, NULL, nullptr);
        if (thread)
            CloseHandle(thread);
        break;
    }
    return TRUE;
}