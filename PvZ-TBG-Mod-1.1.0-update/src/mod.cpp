// PvZ TBG Mod — ядро. Plants vs. Zombies GOTY, Steam, 1.2.0.1096 (en).
// Работает внутри процесса игры. Вся логика выполняется в игровом потоке:
// мы подменяем в таблице импорта Sleep/PeekMessage и вызываем tick() между кадрами,
// поэтому гонок с игровым кодом нет.
#include "plat.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <math.h>
#include "shared.h"

Shared       S;
std::wstring g_dataDir;

static HWND  g_gameWnd  = nullptr;
static DWORD g_mainTid  = 0;
static volatile bool g_ready = false;

// ============================================================================
//  Адреса и смещения (Steam GOTY 1.2.0.1096, данные сверены с PvZ Tools)
// ============================================================================
namespace G {
constexpr uintptr_t LAWN_PTR        = 0x731c50;      // LawnApp**
constexpr DWORD     TIMESTAMP       = 0x4d02b058;    // PE TimeDateStamp этой сборки
// LawnApp
constexpr int APP_FRAME_MS   = 0x4b4;
constexpr int APP_BOARD      = 0x868;
constexpr int APP_MODE       = 0x918;
constexpr int APP_UI         = 0x91c;
constexpr int APP_FREEPLANT  = 0x934;
constexpr int APP_USERDATA   = 0x94c;
// Board
constexpr int B_ZOMBIE       = 0xA8;
constexpr int B_ZOMBIE_MAX   = 0xAC;
constexpr int B_PLANT        = 0xC4;
constexpr int B_PLANT_MAX    = 0xC8;
constexpr int B_SEEDBANK     = 0x15C;
constexpr int B_CHALLENGE    = 0x178;
constexpr int B_SPAWN_LIST   = 0x6CC;                // int[100][50]
constexpr int B_ZALLOWED     = 0x54EC;               // bool[100]
constexpr int B_SCENE        = 0x5564;
constexpr int B_SUNCD        = 0x5550;               // обратный отсчёт до следующего солнца с неба
constexpr int B_SUN          = 0x5578;
constexpr int B_NUMWAVES     = 0x557C;
constexpr int B_CLOCK        = 0x5580;
constexpr int B_CURWAVE      = 0x5594;
constexpr int B_ZCOUNTDOWN   = 0x55B4;
constexpr int B_FOGBLOWN     = 0x5EC;
// Plant (0x14C байт)
constexpr int PL_SIZE = 0x14C, PL_ROW = 0x1C, PL_TYPE = 0x24, PL_COL = 0x28, PL_STATE = 0x3C,
              PL_HP = 0x40, PL_HPMAX = 0x44, PL_STATECD = 0x54, PL_LAUNCHCD = 0x58, PL_LAUNCHRATE = 0x5C,
              PL_IMIT = 0x138, PL_DEAD = 0x141, PL_SQUISH = 0x142, PL_ASLEEP = 0x143;
// Zombie (0x168 байт)
constexpr int ZB_SIZE = 0x168, ZB_TYPE = 0x24, ZB_PHASE = 0x28, ZB_POSX = 0x2C, ZB_EATING = 0x51,
              ZB_FROMWAVE = 0x6C, ZB_CONTROLLED = 0xB8, ZB_HELMTYPE = 0xC4, ZB_BODY = 0xC8,
              ZB_BODYMAX = 0xCC, ZB_HELM = 0xD0, ZB_HELMMAX = 0xD4, ZB_SHIELD = 0xDC,
              ZB_SHIELDMAX = 0xE0, ZB_DEAD = 0xEC;
// SeedBank / SeedPacket
constexpr int SB_CONVEYOR = 0x34C;      // SeedBank::mConveyorBeltCounter (после 10 семян по 0x50)
constexpr int SB_COUNT = 0x24, SB_PACKETS = 0x28, SP_SIZE = 0x50, SP_CD = 0x24, SP_CDMAX = 0x28,
              SP_TYPE = 0x34, SP_IMIT = 0x38, SP_ACTIVE = 0x48, SP_REFRESHING = 0x49, SP_USED = 0x4C;
// PlayerInfo (GOTY)
constexpr int UD_ID = 0x20, UD_NAME = 0x00, UD_ACH = 0x24, UD_ACH_COUNT = 21;   // mId, mName (MSVC string), mEarnedAchievements[21]
constexpr int UD_LEVEL = 0x50, UD_MONEY = 0x54, UD_FINISHED = 0x58, UD_RECORDS = 0x5C,
              UD_UPSELL = 0x374, UD_MINIGAMES = 0x34C, UD_PUZZLE = 0x350, UD_SURVIVAL = 0x364,
              UD_PURCHASES = 0x1EC;
// Состояния растений
constexpr int PS_NOTREADY = 0, PS_CHOMPER_DIGESTING = 13;
// Вызовы игровых функций
constexpr uintptr_t FN_PUT_GRAVE  = 0x42a170;   // edi=row, ebx=col, push Challenge*
constexpr uintptr_t FN_SLEEP      = 0x464b70;   // edi=Plant*, push int
constexpr uintptr_t FN_PICK_BG    = 0x40d5a0;     // esi=Board*
}  // namespace G

#ifdef PVZ_TEST
char* g_testApp = nullptr;
#endif
static char* lawnApp() {
#ifdef PVZ_TEST
    return g_testApp;
#else
    return *(char**)G::LAWN_PTR;
#endif
}
template <class T> static inline T& at(void* b, int o) { return *(T*)((char*)b + o); }
static bool okp(const void* p, size_t n) { return p && (uintptr_t)p > 0x10000 && !IsBadReadPtr(p, n); }

static uint32_t g_rng = 0x9E3779B9u;
static uint32_t rnd() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static int rndN(int n) { return n <= 0 ? 0 : (int)(rnd() % (uint32_t)n); }

void ModSay(const char* m) {
    EnterCriticalSection(&S.lock);
    size_t n = strlen(m); if (n > sizeof(S.msg) - 1) { n = sizeof(S.msg) - 1; while (n > 0 && ((unsigned char)m[n] & 0xC0) == 0x80) n--; }   // не режем символ UTF-8
    memcpy(S.msg, m, n); S.msg[n] = 0;
    LeaveCriticalSection(&S.lock);
}

// ============================================================================
//  Таблицы
// ============================================================================
static const int PLANT_COST[53] = {
    100, 50, 150, 50, 25, 175, 150, 200, 0, 25, 75, 75, 75, 25, 75, 125, 25, 50, 325, 25, 125, 100,
    175, 125, 0, 25, 125, 100, 125, 125, 125, 100, 100, 25, 100, 75, 50, 100, 50, 300, 250, 150, 150,
    225, 200, 50, 125, 500, 0, 50, 400, 0, 200};

static bool isUpgrade(int t)  { return t >= 40 && t <= 47; }
static bool isNocturnal(int t){ return t == 8 || t == 9 || t == 10 || t == 12 || t == 13 || t == 14 ||
                                       t == 15 || t == 24 || t == 31 || t == 42; }
static bool isShooter(int t)  {
    switch (t) {
        case 0: case 5: case 7: case 8: case 10: case 13: case 18: case 24: case 26: case 28: case 29:
        case 32: case 34: case 39: case 40: case 42: case 43: case 44: case 52: return true;
    }
    return false;
}
static int plantCost(int type, int imit) {
    if (type == 48 && imit >= 0 && imit < 53) return PLANT_COST[imit];
    return (type >= 0 && type < 53) ? PLANT_COST[type] : 0;
}
// Стоимость зомби в «Я, зомби» (SeedType 60..74)
static int izBaseCost(int seed) {
    switch (seed) {
        case 60: return 50;  case 61: return 75;  case 62: return 75;  case 63: return 125;
        case 64: return 150; case 65: return 125; case 66: return 125; case 67: return 175;
        case 68: return 150; case 69: return 100; case 70: return 175; case 71: return 200;
        case 72: return 350; case 73: return 300; case 74: return 50;
    }
    return 0;
}

// ============================================================================
//  Патчи кода (байты проверяются перед записью)
// ============================================================================
struct Site { uintptr_t a; uint8_t n; uint8_t on[4]; uint8_t off[4]; };
struct Hack { std::vector<Site> s; int state = -1; bool bad = false; };

static bool hackSet(Hack& h, bool on) {
#ifdef PVZ_TEST
    (void)h; (void)on; return false;
#endif
    if (h.bad) return false;
    if (h.state == (on ? 1 : 0)) return true;
    if (h.state == -1) {
        bool allOn = true;
        for (auto& x : h.s) {
            if (!okp((void*)x.a, x.n)) { h.bad = true; return false; }
            bool isOff = memcmp((void*)x.a, x.off, x.n) == 0;
            bool isOn  = memcmp((void*)x.a, x.on,  x.n) == 0;
            if (!isOff && !isOn) { h.bad = true; return false; }   // чужая версия exe — не трогаем
            if (!isOn) allOn = false;
        }
        h.state = allOn ? 1 : 0;
        if (h.state == (on ? 1 : 0)) return true;
    }
    for (auto& x : h.s) {
        DWORD old;
        VirtualProtect((void*)x.a, x.n, PAGE_EXECUTE_READWRITE, &old);
        memcpy((void*)x.a, on ? x.on : x.off, x.n);
        VirtualProtect((void*)x.a, x.n, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)x.a, x.n);
    }
    h.state = on ? 1 : 0;
    return true;
}
static Site mk(uintptr_t a, std::initializer_list<uint8_t> on, std::initializer_list<uint8_t> off) {
    Site s{}; s.a = a; s.n = (uint8_t)on.size();
    int i = 0; for (auto v : on)  s.on[i++] = v;
    i = 0;     for (auto v : off) s.off[i++] = v;
    return s;
}
static Hack hkNoSunCap, hkAutoCollect, hkMushAwake, hkStopSpawn, hkPlaceAnywhere, hkBgRun, hkInvincible;

static void buildHacks() {
    hkNoSunCap.s     = {mk(0x41f4e5, {0xeb}, {0x7e})};
    hkAutoCollect.s  = {mk(0x4352f2, {0xeb}, {0x75})};
    hkMushAwake.s    = {mk(0x4641a2, {0xeb}, {0x74})};
    hkStopSpawn.s    = {mk(0x42a12c, {0xeb}, {0x74})};
    hkBgRun.s        = {mk(0x5d87c9, {0xeb, 0x00}, {0x74, 0x40})};
    hkPlaceAnywhere.s= {mk(0x413350, {0x81}, {0x84}), mk(0x43d100, {0xeb}, {0x74}), mk(0x42dec9, {0x8d}, {0x84})};
    hkInvincible.s   = {
        mk(0x5447a1, {0x46, 0x40, 0x00}, {0x46, 0x40, 0xfc}), mk(0x4207df, {0xeb}, {0x74}),
        mk(0x53beda, {0xeb}, {0x75}), mk(0x474ffb, {0x90, 0x90, 0x90}, {0x29, 0x50, 0x40}),
        mk(0x4757b9, {0x90, 0x90, 0x90}, {0x29, 0x4e, 0x40}), mk(0x5433ab, {0xeb}, {0x74}),
        mk(0x46511a, {0x70}, {0x75}), mk(0x464f76, {0x00}, {0xce}), mk(0x468f60, {0xc2, 0x04, 0x00}, {0x53, 0x55, 0x8b})};
}

// ============================================================================
//  Вызовы функций игры (из игрового потока, как это делает PvZ Tools)
// ============================================================================
#ifdef PVZ_TEST
int t_graves = 0, t_sleeps = 0, t_wakes = 0, t_bg = 0;
static void gamePutGrave(void*, int, int) { t_graves++; }
static void gameSetSleep(void*, int s) { if (s) t_sleeps++; else t_wakes++; }
static void gamePickBackground(void*) { t_bg++; }
#else
static void gamePutGrave(void* challenge, int row, int col) {
    __asm__ volatile("pushl %[ch]\n\tmovl %[fn],%%eax\n\tcall *%%eax"
                     : "+D"(row), "+b"(col)
                     : [ch] "g"(challenge), [fn] "i"(G::FN_PUT_GRAVE)
                     : "eax", "ecx", "edx", "esi", "memory", "cc");
}
static void gameSetSleep(void* plant, int sleep) {
    __asm__ volatile("pushl %[s]\n\tmovl %[fn],%%eax\n\tcall *%%eax"
                     : "+D"(plant)
                     : [s] "g"(sleep), [fn] "i"(G::FN_SLEEP)
                     : "eax", "ecx", "edx", "ebx", "esi", "memory", "cc");
}
static void gamePickBackground(void* board) {
    __asm__ volatile("movl %[fn],%%eax\n\tcall *%%eax"
                     : "+S"(board)
                     : [fn] "i"(G::FN_PICK_BG)
                     : "eax", "ecx", "edx", "ebx", "edi", "memory", "cc");
}
#endif

// ============================================================================
//  Состояние уровня
// ============================================================================
struct Lvl {
    char* board = nullptr;
    int   lastClock = -1;
    bool  baselined = false, bankInit = false, wavesInit = false;
    int   lastWave = -1;
    float accAtk = 0, accSeed = 0, accWave = 0, accSunVal = 0, accSky = 0, accSunPl = 0, accMoney = 0;
    bool  prevP[2048], prevZ[2048], hpDone[2048], noBonus[2048];
    float lastZX[2048];
    int   usedPk[10];
    int   lastSun = -1, lastMoney = -1;
    // конвейер
    int   bankN = -1, convCtr = -1, convDown = 0, convUp = 0; bool convSeen = false; float accConv = 0;
    int   maxSun = 0;                       // наибольшее солнце за уровень: на конвейерных уровнях солнца нет вовсе
    bool  convLevel() const { return convSeen || maxSun <= 0; }     // «солнечной экономики» нет — списывать и отменять нечего
    // автосинхронизация состава волн с настройками
    int   waveSig = 0; bool waveTouched = false;
    float accAtkT[53] = {}, accSeedT[49] = {}, accFirst = 0;   // накопители дробных тактов по типам растений / семян, первая волна
    int   convCycle = 0, convLast = -1;                          // конвейер: счётчик «по кругу», прошлое положенное семя
    bool  startDone = false;
    float accChomp = 0, accMine = 0;                              // дробные такты чомпера и мины
    std::vector<int> orig;      // исходные волны (до наших правок)
    int   origWaves = 0;
    Lvl() { memset(prevP, 0, sizeof prevP); memset(prevZ, 0, sizeof prevZ); memset(hpDone, 0, sizeof hpDone); memset(noBonus, 0, sizeof noBonus);
            memset(lastZX, 0, sizeof lastZX); memset(usedPk, 0, sizeof usedPk); }
};
static Lvl* L = nullptr;
static bool g_pendingRandomSet = false;
static DWORD g_pendingAt = 0;                  // когда поставили отложенную выдачу набора (залипший флаг сбрасываем по таймеру)
static bool g_moneyGiven = false;              // деньги только что выданы кнопкой — не считать их подобранными монетами
static bool g_weSetCheat = false;
static int  g_frameDefault = 0;
static bool g_frameChanged = false;

static bool needNoSunCapHack(const Cfg& c) { return c.noSunCap || (c.sunCapOn && c.sunCapVal > 9990); }
static int sunCap(const Cfg& c) { return c.sunCapOn ? c.sunCapVal : (c.noSunCap ? 99990 : 9990); }
static int effPlant(int type, int imit) { return (type == 48 && imit >= 0 && imit < 48) ? imit : type; }   // имитатор считается тем растением, которое он копирует
static void setSun(char* board, int v, const Cfg& c) {
    int cap = sunCap(c), cur = at<int>(board, G::B_SUN);
    if (v > cap) v = std::max(cap, std::min(v, cur));        // выше потолка не поднимаем, но и не отбираем то, что уже есть сверх него
    at<int>(board, G::B_SUN) = v < 0 ? 0 : v;
}
static int rowCount(char* board) { int sc = at<int>(board, G::B_SCENE); return (sc == 2 || sc == 3) ? 6 : 5; }

// ---------- Волны зомби ----------
static void universeFor(int scene, std::vector<int>& u) {
    u = {0, 2, 3, 4, 5, 6, 7, 8, 12, 15, 16, 17, 18, 20, 21, 23};
    if (scene == 2 || scene == 3) { u.push_back(10); u.push_back(11); u.push_back(14); }
    if (scene == 4 || scene == 5) { u.push_back(22); u.erase(std::remove(u.begin(), u.end(), 17), u.end()); }
}

static void rebuildWaves(char* board, const Cfg& c, int startWave) {
    int nw = at<int>(board, G::B_NUMWAVES);
    if (nw < 1 || nw > 100 || !L) return;
    int* list = (int*)(board + G::B_SPAWN_LIST);
    if (!okp(list, 100 * 50 * 4)) return;
    if (L->origWaves != nw) {        // кэшируем оригинал один раз
        L->orig.assign(list, list + nw * 50);
        L->origWaves = nw;
    }
    L->waveTouched = true;
    std::vector<int> S2;             // уникальные типы уровня (для «равных» зомби)
    for (int i = 0; i < nw * 50; i++) {
        int t = L->orig[i];
        if (t < 0 || t == 1 || t == 9 || t == 19 || t == 24 || t == 25 || t > 26) continue;
        if (std::find(S2.begin(), S2.end(), t) == S2.end()) S2.push_back(t);
    }
    std::vector<int> uni; universeFor(at<int>(board, G::B_SCENE), uni);
    bool* allowed = (bool*)(board + G::B_ZALLOWED);

    for (int w = std::max(0, startWave); w < nw; w++) {
        std::vector<int> flags, normals;
        for (int k = 0; k < 50; k++) {
            int t = L->orig[w * 50 + k];
            if (t < 0) break;
            (t == 1 ? flags : normals).push_back(t);
        }
        int n = (int)normals.size();
        int newN = n;
        if (c.zCountOn) {
            newN = (int)lroundf(n * c.zCount);
            if (c.zCount > 0.f && n > 0 && newN < 1) newN = 1;
        }
        newN = std::min(newN, 49 - (int)flags.size());
        if (newN < 0) newN = 0;
        std::vector<int> out;
        if (c.randZombies && !uni.empty() && n > 0) {
            for (int i = 0; i < newN; i++) out.push_back(uni[rndN((int)uni.size())]);
        } else if (c.zEqual && !S2.empty() && n > 0) {
            for (int i = 0; i < newN; i++) out.push_back(S2[rndN((int)S2.size())]);
        } else {
            out = normals;
            for (int i = (int)out.size() - 1; i > 0; i--) std::swap(out[i], out[rndN(i + 1)]);
            if (newN < (int)out.size()) out.resize(newN);
            while ((int)out.size() < newN && n > 0) out.push_back(normals[rndN(n)]);
        }
        int idx = 0;
        for (int f : flags) list[w * 50 + idx++] = f;
        for (int t : out) { list[w * 50 + idx++] = t; if (t >= 0 && t < 100) allowed[t] = true; }
        if (idx < 50) list[w * 50 + idx] = -1;
    }
}

// Возвращает исходный состав волн (если мы его меняли, а потом настройку выключили).
static void restoreWaves(char* board, int startWave) {
    if (!L || !L->waveTouched || L->origWaves < 1) { if (L) L->waveTouched = false; return; }
    int nw = std::min(at<int>(board, G::B_NUMWAVES), L->origWaves);
    int* list = (int*)(board + G::B_SPAWN_LIST);
    if (!okp(list, 100 * 50 * 4)) return;
    for (int w = std::max(0, startWave); w < nw; w++)
        for (int k = 0; k < 50; k++) list[w * 50 + k] = L->orig[w * 50 + k];
    L->waveTouched = false;
}

// ---------- Случайные растения ----------
static void candidatePlants(const Cfg& c, char* bank, int count, int exceptSlot, std::vector<int>& out) {
    bool inSlot[53] = {};
    for (int i = 0; i < count; i++) {
        int t = at<int>(bank, G::SB_PACKETS + i * G::SP_SIZE + G::SP_TYPE);
        if (t >= 0 && t < 53) inSlot[t] = true;
    }
    (void)exceptSlot;
    for (int t = 0; t < 48; t++) {
        if (inSlot[t]) continue;
        if (isUpgrade(t) && !c.premiumNoBase) continue;
        out.push_back(t);
    }
}
static void applyRandomSet(char* board, const Cfg& c) {
    char* bank = *(char**)(board + G::B_SEEDBANK);
    if (!okp(bank, 0x350)) return;
    int n = at<int>(bank, G::SB_COUNT);
    if (n < 1 || n > 10) return;
    int want = std::min(c.randSetCount, n);
    std::vector<int> pool;
    for (int t = 0; t < 48; t++) if (!(isUpgrade(t) && !c.premiumNoBase)) pool.push_back(t);
    std::vector<int> pick;
    if (c.randKeepSun) {
        int night = (at<int>(board, G::B_SCENE) == 1 || at<int>(board, G::B_SCENE) == 3);
        int sunPlant = (night && rndN(2)) ? 9 : 1;
        pick.push_back(sunPlant);
        pool.erase(std::remove(pool.begin(), pool.end(), sunPlant), pool.end());
    }
    while ((int)pick.size() < want && !pool.empty()) {
        int k = rndN((int)pool.size());
        pick.push_back(pool[k]);
        pool.erase(pool.begin() + k);
    }
    for (int i = 0; i < n; i++) {
        char* pk = bank + G::SB_PACKETS + i * G::SP_SIZE;
        if (i < (int)pick.size()) {
            at<int>(pk, G::SP_TYPE) = pick[i];
            at<int>(pk, G::SP_IMIT) = -1;
            at<int>(pk, G::SP_CD) = 0;
            at<uint8_t>(pk, G::SP_ACTIVE) = 1;
            at<uint8_t>(pk, G::SP_REFRESHING) = 0;
        }
    }
    ModSay("Случайный набор растений выдан.");
}

// ---------- События уровня ----------
static void evGraves(char* board, const Cfg& c) {
    char* ch = *(char**)(board + G::B_CHALLENGE);
    if (!okp(ch, 0x100)) return;
    int rows = rowCount(board);
    char* plants = *(char**)(board + G::B_PLANT);
    int pmax = at<int>(board, G::B_PLANT_MAX);
    if (pmax < 0 || pmax > 2048 || !okp(plants, (size_t)pmax * G::PL_SIZE)) return;
    for (int k = 0, tries = 0; k < c.evGraveCount && tries < 40; tries++) {
        int row = rndN(rows), col = 3 + rndN(6);
        bool busy = false;
        for (int i = 0; i < pmax; i++) {
            char* p = plants + i * G::PL_SIZE;
            if (at<uint8_t>(p, G::PL_DEAD) || at<uint8_t>(p, G::PL_SQUISH)) continue;
            if (at<int>(p, G::PL_ROW) == row && at<int>(p, G::PL_COL) == col) { busy = true; break; }
        }
        if (busy) continue;
        gamePutGrave(ch, row, col);
        k++;
    }
}
static void evDayNight(char* board, const Cfg& c) {
    int sc = at<int>(board, G::B_SCENE);
    if (sc != 0 && sc != 1) return;            // только 5-рядные газонные сцены (день/ночь)
    int ns = 1 - sc;
    at<int>(board, G::B_SCENE) = ns;
    gamePickBackground(board);
    char* plants = *(char**)(board + G::B_PLANT);
    int pmax = at<int>(board, G::B_PLANT_MAX);
    if (pmax < 0 || pmax > 2048 || !okp(plants, (size_t)pmax * G::PL_SIZE)) return;
    for (int i = 0; i < pmax; i++) {
        char* p = plants + i * G::PL_SIZE;
        if (at<uint8_t>(p, G::PL_DEAD) || at<uint8_t>(p, G::PL_SQUISH)) continue;
        int t = at<int>(p, G::PL_TYPE);
        if (!isNocturnal(t)) continue;
        bool asleep = at<uint8_t>(p, G::PL_ASLEEP) != 0;
        if (ns == 0 && !asleep && !c.mushAwake) gameSetSleep(p, 1);   // наступил день — грибы засыпают
        if (ns == 1 && asleep) gameSetSleep(p, 0);                    // наступила ночь — просыпаются
    }
    ModSay(ns == 0 ? "Событие: наступил день — грибы уснули." : "Событие: наступила ночь.");
}

// ---------- Клик по кнопке главного меню ----------
static int g_clickStage = 0, g_clickX = 0, g_clickY = 0;
static DWORD g_clickAt = 0;
static void beginClick(int x, int y) { g_clickX = x; g_clickY = y; g_clickStage = 1; g_clickAt = GetTickCount() + 350; }
static void pumpClick() {
    if (!g_clickStage || GetTickCount() < g_clickAt || !g_gameWnd) return;
    RECT rc; GetClientRect(g_gameWnd, &rc);
    double cw = rc.right, ch = rc.bottom;
    double s = std::min(cw / 800.0, ch / 600.0);
    double ox = (cw - 800 * s) / 2, oy = (ch - 600 * s) / 2;
    LPARAM lp = MAKELPARAM((int)(ox + g_clickX * s), (int)(oy + g_clickY * s));
    if (g_clickStage == 1) PostMessageW(g_gameWnd, WM_MOUSEMOVE, 0, lp);
    if (g_clickStage == 2) PostMessageW(g_gameWnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    if (g_clickStage == 3) PostMessageW(g_gameWnd, WM_LBUTTONUP, 0, lp);
    g_clickStage = (g_clickStage >= 3) ? 0 : g_clickStage + 1;
    g_clickAt = GetTickCount() + 90;
}

// ---------- Профиль игрока ----------
static void doUnlock(char* app, const Cfg& c) {
    char* ud = *(char**)(app + G::APP_USERDATA);
    if (!okp(ud, 0x400)) { ModSay("Профиль не загружен — выберите игрока в главном меню."); return; }
    if (c.ulStore)   at<int>(ud, G::UD_UPSELL) = 1;
    if (c.ulAlmanac || c.ulStore) { if (at<int>(ud, G::UD_FINISHED) < 1) at<int>(ud, G::UD_FINISHED) = 1; }
    if (c.ulModes) {
        at<int>(ud, G::UD_MINIGAMES) = 1; at<int>(ud, G::UD_PUZZLE) = 1; at<int>(ud, G::UD_SURVIVAL) = 1;
        if (at<int>(ud, G::UD_FINISHED) < 1) at<int>(ud, G::UD_FINISHED) = 1;
        int* r = (int*)(ud + G::UD_RECORDS);
        static const int surv[10] = {5, 5, 5, 5, 5, 10, 10, 10, 10, 10};
        for (int i = 0; i < 10; i++) r[i] = surv[i];
        r += 10 + 5;                                   // «Выживание» + бесконечное
        for (int i = 0; i < 20; i++) r[i] = 1;         // мини-игры
        r += 20 + 14 + 1;                              // скрытые мини-игры + дерево мудрости
        for (int i = 0; i < 9; i++) r[i] = 1;          // разбей вазу
        r += 9 + 1;
        for (int i = 0; i < 9; i++) r[i] = 1;          // Я, зомби
    }
    if (c.ulBuyAll) {
        int* p = (int*)(ud + G::UD_PURCHASES);
        for (int i : {0, 1, 2, 3, 4, 5, 6, 7, 8, 13, 16, 17, 18, 19, 20, 22, 23, 24, 25, 27, 29}) p[i] = 1;
        p[21] = 4;
    }
    ModSay("Разблокировка применена. Вернитесь в главное меню, чтобы кнопки обновились.");
}
static void givePurchase(char* app, int idx, int n) {
    char* ud = *(char**)(app + G::APP_USERDATA);
    if (!okp(ud, 0x400)) { ModSay("Профиль не загружен."); return; }
    ((int*)(ud + G::UD_PURCHASES))[idx] = 1000 + n;     // 1000 + количество (PURCHASE_COUNT_OFFSET)
    ModSay("Выдано.");
}


// ---------- Игрок, достижения, выдача растений ----------
// Имя игрока: MSVC std::string (proxy 4 байта, буфер/указатель, size, reserve).
static void playerName(char* ud, char* out, size_t n) {
    out[0] = 0;
    if (!okp(ud, 0x30)) return;
    int size = at<int>(ud, G::UD_NAME + 0x14), res = at<int>(ud, G::UD_NAME + 0x18);
    if (size <= 0 || size > 30 || res < size || res > 4096) return;
    const char* src = res >= 16 ? *(const char**)(ud + G::UD_NAME + 4) : ud + G::UD_NAME + 4;
    if (!okp(src, (size_t)size)) return;
    size_t k = 0;
    for (int i = 0; i < size && k + 1 < n; i++) { unsigned char ch = (unsigned char)src[i]; out[k++] = (ch < 32) ? '?' : (char)ch; }
    out[k] = 0;
}
static int  popcount32(unsigned v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }
static void giveAchievements(char* app, unsigned mask, bool grant) {
    char* ud = *(char**)(app + G::APP_USERDATA);
    if (!okp(ud, 0x400)) { ModSay("Профиль не загружен — выберите игрока в главном меню."); return; }
    bool* a = (bool*)(ud + G::UD_ACH);
    for (int i = 0; i < G::UD_ACH_COUNT; i++) if (mask & (1u << i)) a[i] = grant;
    char b[260]; snprintf(b, sizeof b, grant ? "Достижений выдано: %d. Они покажутся в окне наград, список — в «Достижениях» главного меню."
                                             : "Достижений забрано: %d.", popcount32(mask & ((1u << G::UD_ACH_COUNT) - 1)));
    ModSay(b);
}
// Премиум-растения (семя 40..48) — открываются покупкой в магазине: ставим флаг покупки.
static void givePremiumPlants(char* app, unsigned long long mask) {
    char* ud = *(char**)(app + G::APP_USERDATA);
    if (!okp(ud, 0x400)) { ModSay("Профиль не загружен — выберите игрока в главном меню."); return; }
    int n = 0, skipped = 0;
    for (int t = 0; t < 49; t++) {
        if (!(mask >> t & 1ull)) continue;
        if (t >= 40) { ((int*)(ud + G::UD_PURCHASES))[t - 40] = 1; n++; } else skipped++;
    }
    char b[320];
    if (skipped) snprintf(b, sizeof b, "Открыто премиум-растений: %d. Обычные открываются только все сразу — кнопкой «Все обычные», либо кладите их в слоты боя.", n);
    else snprintf(b, sizeof b, "Открыто премиум-растений: %d.", n);
    ModSay(b);
}
// Обычные растения игра открывает «префиксом» по прогрессу приключения, поэтому поштучно их не открыть.
// Выдаём все сразу: флаг «приключение пройдено» (как и «Разблокировать»).
static void giveAllBasePlants(char* app) {
    char* ud = *(char**)(app + G::APP_USERDATA);
    if (!okp(ud, 0x400)) { ModSay("Профиль не загружен — выберите игрока в главном меню."); return; }
    if (at<int>(ud, G::UD_FINISHED) < 1) at<int>(ud, G::UD_FINISHED) = 1;
    ModSay("Все обычные растения открыты (флаг «приключение пройдено»). Вернитесь в главное меню, чтобы игра подхватила.");
}
// Положить выбранные растения в слоты семян текущего боя (любые, независимо от открытия).
static void putPlantsInSlots(char* board, unsigned long long mask) {
    char* bank = *(char**)(board + G::B_SEEDBANK);
    if (!okp(bank, 0x350)) { ModSay("Слоты выдаются, когда бой начался."); return; }
    int n = at<int>(bank, G::SB_COUNT);
    if (n < 1 || n > 10) return;
    int slot = 0;
    for (int t = 0; t < 48 && slot < n; t++) {
        if (!(mask >> t & 1ull)) continue;
        char* pk = bank + G::SB_PACKETS + slot * G::SP_SIZE;
        at<int>(pk, G::SP_TYPE) = t; at<int>(pk, G::SP_IMIT) = -1; at<int>(pk, G::SP_CD) = 0;
        at<uint8_t>(pk, G::SP_ACTIVE) = 1; at<uint8_t>(pk, G::SP_REFRESHING) = 0;
        slot++;
    }
    char b[200]; snprintf(b, sizeof b, slot ? "Растений положено в слоты: %d." : "Ничего не выбрано.", slot);
    ModSay(b);
}

// ---------- Прохождение сборок ----------
// Каждая сборка привязана к игровому профилю с точным именем (например «TBG Hard»). Когда в игре выбран НОВЫЙ игрок с таким именем,
// мод сам начинает собирать прогресс и замораживает меню. Выйти можно, выбрав другого игрока, либо сбросив прохождение досрочно.
static int g_noAutoPid = -1;      // игрок, на котором прохождение только что сбросили: заново само не стартует, пока не выбран другой игрок
static int g_hintPid = -1;        // игрок, которому уже показали пояснение (чтобы не повторять каждый тик)
static void runHint(int pid, const char* msg) { if (g_hintPid != pid) { g_hintPid = pid; ModSay(msg); } }

// Возвращает номер начатой сборки или -1.
static int runAutoStart(char* ud) {
    int pid = at<int>(ud, G::UD_ID);
    char nm[40]; playerName(ud, nm, sizeof nm);
    int idx = buildByProfile(nm);
    if (idx < 0 || pid == g_noAutoPid) return -1;
    char b[260];
    EnterCriticalSection(&S.lock);
    RunState cur = S.runs[idx];
    LeaveCriticalSection(&S.lock);
    if (cur.active && cur.done) {
        if (cur.playerId == pid) return -1;              // свой пройденный профиль: просто разморожено
        snprintf(b, sizeof b, "Сборка «%s» уже пройдена на профиле «%s». Чтобы пройти заново, сбросьте её во вкладке «Сборки TBG» и создайте нового игрока.", kBuilds[idx].name, cur.player);
        runHint(pid, b); return -1;
    }
    if (cur.active) {                                    // идёт на другом профиле (например, профиль удалили и создали заново)
        snprintf(b, sizeof b, "Сборка «%s» уже идёт на профиле «%s». Сначала сбросьте её во вкладке «Сборки TBG».", kBuilds[idx].name, cur.player);
        runHint(pid, b); return -1;
    }
    if (at<int>(ud, G::UD_LEVEL) > 1 || at<int>(ud, G::UD_FINISHED) > 0) {
        snprintf(b, sizeof b, "Профиль «%s» уже не новый (есть прогресс). Удалите его и создайте нового игрока с этим именем.", nm);
        runHint(pid, b); return -1;
    }
    EnterCriticalSection(&S.lock);
    RunState& r = S.runs[idx]; r = RunState();
    r.active = 1; r.playerId = pid; strncpy(r.player, nm, sizeof r.player - 1);
    LeaveCriticalSection(&S.lock);
    InterlockedIncrement(&S.runSeq);
    snprintf(b, sizeof b, "Сборка «%s» началась на профиле «%s». Мод заморожен до конца приключения.", kBuilds[idx].name, nm);
    ModSay(b);
    return idx;
}

// ============================================================================
//  Основной тик (игровой поток)
// ============================================================================
static void tick() {
    Cfg c;
    EnterCriticalSection(&S.lock); c = S.cfg; LeaveCriticalSection(&S.lock);
    char* app = lawnApp();
    if (!okp(app, 0x1000)) return;
    int ui = at<int>(app, G::APP_UI);
    S.ui = ui; S.gameMode = at<int>(app, G::APP_MODE);
    pumpClick();

    // --- сборки: запуск/сброс, привязка к игроку, прогресс, заморозка настроек ---
    char* udp = *(char**)(app + G::APP_USERDATA);
    bool udOk = okp(udp, 0x400);
    int curPid = udOk ? at<int>(udp, G::UD_ID) : -1;
    if (curPid != g_noAutoPid) g_noAutoPid = -1;               // выбрали другого игрока — автозапуск снова разрешён
    if (udOk) {
        char nm[40]; playerName(udp, nm, sizeof nm);
        EnterCriticalSection(&S.lock); if (strcmp(S.curPlayer, nm) != 0) strcpy(S.curPlayer, nm); LeaveCriticalSection(&S.lock);
    }
    if (InterlockedExchange(&S.cmd[CMD_RUN_STOP], 0)) {
        int idx = (int)S.runStopArg;
        if (idx >= 0 && idx < kBuildCount) {
            EnterCriticalSection(&S.lock);
            if (udOk && S.runs[idx].active && S.runs[idx].playerId == curPid) g_noAutoPid = curPid;     // этот игрок остаётся выбранным: сам не стартуем
            S.runs[idx] = RunState();
            LeaveCriticalSection(&S.lock);
            InterlockedIncrement(&S.runSeq); ModSay("Прохождение сборки сброшено. Мод разморожен.");
        }
    }
    int frozen = -1;
    if (udOk) {
        int pid = curPid;
        EnterCriticalSection(&S.lock);
        for (int i = 0; i < kMaxRuns; i++) if (S.runs[i].active && !S.runs[i].done && S.runs[i].playerId == pid) frozen = i;
        LeaveCriticalSection(&S.lock);
        if (frozen < 0) frozen = runAutoStart(udp);
        if (frozen >= 0) {                                   // процент прохождения приключения: 50 уровней по 2%
            int level = at<int>(udp, G::UD_LEVEL);
            bool fin = at<int>(udp, G::UD_FINISHED) > 0 || level > 50;
            int pct = fin ? 100 : (level - 1) * 2;
            if (!fin && ui == 3 && S.gameMode == 0) {        // доля волн текущего уровня
                char* bd = *(char**)(app + G::APP_BOARD);
                if (okp(bd, 0x5700)) {
                    int nw = at<int>(bd, G::B_NUMWAVES), cw = at<int>(bd, G::B_CURWAVE);
                    if (nw > 0 && nw <= 100 && cw >= 0 && cw <= nw) pct += (int)(2.0f * cw / nw);
                }
            }
            if (!fin && pct > 99) pct = 99;
            if (pct < 0) pct = 0;
            static DWORD lastT = 0; static int accMs = 0, sinceSave = 0;                       // время боя под заморозкой
            DWORD nowT = GetTickCount(); int dt = (int)(nowT - lastT); lastT = nowT;
            bool addSec = false;
            if (ui == 3 && dt > 0 && dt < 1000) { accMs += dt; if (accMs >= 1000) { accMs -= 1000; addSec = true; } }
            EnterCriticalSection(&S.lock);
            RunState& r = S.runs[frozen];
            bool ch = false;
            if (addSec) { r.secs++; if (++sinceSave >= 15) { sinceSave = 0; ch = true; } }
            if (pct > r.pct) { r.pct = pct; ch = true; }
            if (fin) { r.done = 1; r.pct = 100; ch = true; }
            LeaveCriticalSection(&S.lock);
            if (ch) InterlockedIncrement(&S.runSeq);
            if (fin) { char b[160]; snprintf(b, sizeof b, "Сборка «%s» пройдена! Мод разморожен.", kBuilds[frozen].name); ModSay(b); frozen = -1; }
        }
    }
    S.frozenBuild = frozen;
    if (g_pendingRandomSet && (frozen >= 0 || GetTickCount() - g_pendingAt > 120000)) g_pendingRandomSet = false;
    if (frozen >= 0) {                                       // заморозка: действуют только настройки сборки
        c = buildCfg(frozen);
        for (int k = 0; k < CMD_COUNT; k++)
            if (k != CMD_RUN_STOP && InterlockedExchange(&S.cmd[k], 0))
                ModSay("Мод заморожен сборкой: эта команда недоступна до конца прохождения.");
    }

    // --- патчи кода ---
    hackSet(hkNoSunCap, needNoSunCapHack(c));
    hackSet(hkAutoCollect, c.autoCollect);
    hackSet(hkMushAwake, c.mushAwake);
    hackSet(hkStopSpawn, c.stopSpawn);
    hackSet(hkPlaceAnywhere, c.placeAnywhere);
    hackSet(hkBgRun, c.bgRun);
    hackSet(hkInvincible, c.invPlants);
    S.hacksBad = hkNoSunCap.bad + hkAutoCollect.bad + hkMushAwake.bad + hkStopSpawn.bad +
                 hkPlaceAnywhere.bad + hkBgRun.bad + hkInvincible.bad;

    // --- скорость игры ---
    int& frameMs = at<int>(app, G::APP_FRAME_MS);
    if (!g_frameDefault && frameMs > 0 && frameMs < 100) g_frameDefault = frameMs;
    if (g_frameDefault) {
        if (c.gameSpeedOn && fabsf(c.gameSpeed - 1.f) > 0.01f) {
            int ms = (int)lroundf((float)g_frameDefault / c.gameSpeed);
            frameMs = ms < 1 ? 1 : (ms > 100 ? 100 : ms);
            g_frameChanged = true;
        } else if (g_frameChanged) { frameMs = g_frameDefault; g_frameChanged = false; }
    }

    // --- команды, не требующие поля боя ---
    if (InterlockedExchange(&S.cmd[CMD_UNLOCK], 0)) doUnlock(app, c);
    if (InterlockedExchange(&S.cmd[CMD_GIVE_MONEY], 0)) {
        char* ud = *(char**)(app + G::APP_USERDATA);
        if (okp(ud, 0x400)) { at<int>(ud, G::UD_MONEY) = std::min(at<int>(ud, G::UD_MONEY) + c.moneyGive / 10, 99999); ModSay("Монеты выданы."); g_moneyGiven = true; }
        else ModSay("Профиль не загружен.");
    }
    if (InterlockedExchange(&S.cmd[CMD_GIVE_FERT], 0)) givePurchase(app, 14, c.fertGive);
    if (InterlockedExchange(&S.cmd[CMD_GIVE_BUG],  0)) givePurchase(app, 15, c.bugGive);
    if (InterlockedExchange(&S.cmd[CMD_GIVE_CHOC], 0)) givePurchase(app, 26, c.chocGive);
    if (InterlockedExchange(&S.cmd[CMD_GIVE_TREE], 0)) givePurchase(app, 28, c.treeGive);
    if (InterlockedExchange(&S.cmd[CMD_ACH_GIVE], 0))   giveAchievements(app, (unsigned)S.achMask, true);
    if (InterlockedExchange(&S.cmd[CMD_ACH_REVOKE], 0)) giveAchievements(app, (unsigned)S.achMask, false);
    auto plantMaskNow = []() { return ((unsigned long long)(unsigned)S.plantHi << 32) | (unsigned)S.plantLo; };
    if (InterlockedExchange(&S.cmd[CMD_PLANTS_PERMA], 0))   givePremiumPlants(app, plantMaskNow());
    if (InterlockedExchange(&S.cmd[CMD_PLANTS_ALLBASE], 0)) giveAllBasePlants(app);

    auto startLevel = [&](int lvl, bool andRandomSet) {
        if (ui != 1) { ModSay("Запуск работает из главного меню игры."); return; }
        char* ud = *(char**)(app + G::APP_USERDATA);
        if (!okp(ud, 0x400)) { ModSay("Сначала выберите игрока в главном меню."); return; }
        at<int>(ud, G::UD_LEVEL) = lvl;
        if (andRandomSet) { g_pendingRandomSet = true; g_pendingAt = GetTickCount(); }
        beginClick(600, 105);        // кнопка «Приключение»
        char b[160]; snprintf(b, sizeof b, "Запуск уровня %d-%d… если клик не попал — нажмите «Приключение» вручную.", (lvl - 1) / 10 + 1, (lvl - 1) % 10 + 1);
        ModSay(b);
    };
    if (InterlockedExchange(&S.cmd[CMD_START], 0)) startLevel(c.startLevel, false);
    if (InterlockedExchange(&S.cmd[CMD_START_RANDOM], 0)) startLevel(1 + rndN(50), false);

    // --- поле боя ---
    char* board = *(char**)(app + G::APP_BOARD);
    bool inBoard = (ui == 2 || ui == 3) && okp(board, 0x5700);
    if (!inBoard) {
        S.sun = S.wave = S.numWaves = 0;
        if (g_weSetCheat) { at<int>(app, G::APP_FREEPLANT) = 0; g_weSetCheat = false; }
        if (L) { delete L; L = nullptr; }
        // команды, которым нужен бой, не «зависают»
        for (int k : {CMD_GIVE_SUN, CMD_RANDOM_SET, CMD_REROLL_WAVES, CMD_KILL_ZOMBIES, CMD_HP_EXISTING, CMD_PLANTS_SLOTS})
            if (InterlockedExchange(&S.cmd[k], 0)) ModSay("Эта команда работает во время боя.");
        if (InterlockedExchange(&S.cmd[CMD_RANDOM_SET_AND_START], 0)) startLevel(c.startLevel, true);
        return;
    }
    if (InterlockedExchange(&S.cmd[CMD_RANDOM_SET_AND_START], 0)) { if (ui == 3) applyRandomSet(board, c); else { g_pendingRandomSet = true; g_pendingAt = GetTickCount(); } }

    int clock = at<int>(board, G::B_CLOCK);
    if (!L || L->board != board || clock < L->lastClock) {
        if (L) delete L;
        L = new Lvl(); L->board = board; S.zSunTotal = 0;
    }
    int delta = (L->lastClock >= 0) ? clock - L->lastClock : 0;
    if (delta < 0 || delta > 8) delta = 0;
    L->lastClock = clock;

    int numWaves = at<int>(board, G::B_NUMWAVES);
    int curWave  = at<int>(board, G::B_CURWAVE);
    bool wavesOk = numWaves >= 0 && numWaves <= 100 && curWave >= 0 && curWave <= 100;
    S.sun = at<int>(board, G::B_SUN); S.wave = curWave; S.numWaves = numWaves; S.scene = at<int>(board, G::B_SCENE);

    // --- «все растения бесплатны» / «премиум без носителя» (флаг лёгкой посадки игры) ---
    bool needCheat = c.freePlants || c.premiumNoBase;
    if (needCheat) { at<int>(app, G::APP_FREEPLANT) = 1; g_weSetCheat = true; }
    else if (g_weSetCheat) { at<int>(app, G::APP_FREEPLANT) = 0; g_weSetCheat = false; }

    int sun = at<int>(board, G::B_SUN);
    if (sun > L->maxSun) L->maxSun = sun;
    bool izMode = S.gameMode >= 61 && S.gameMode <= 70;

    // --- стартовое солнце: один раз при появлении поля (только там, где солнце вообще есть) ---
    if (!L->startDone) {
        L->startDone = true;
        // только для нового уровня: продолженная сохранёнка (большой счётчик времени) не сбрасывается
        if (c.startSunOn && !izMode && sun > 0 && clock < 500) { setSun(board, c.startSun, c); sun = at<int>(board, G::B_SUN); if (sun > L->maxSun) L->maxSun = sun; }
    }

    // --- ценность монет: деньги профиля растут только от подобранных монет (в бою) ---
    if (udOk) {
        int m = at<int>(udp, G::UD_MONEY);
        if (g_moneyGiven) { g_moneyGiven = false; L->lastMoney = -1; }          // выданные кнопкой деньги в расчёт монет не входят
        if (c.coinValOn && fabsf(c.coinVal - 1.f) > 0.01f && ui == 3 && L->lastMoney >= 0 && m > L->lastMoney && m - L->lastMoney <= 200) {
            int d = m - L->lastMoney; float want = d * c.coinVal + L->accMoney; int give = (int)floorf(want + 0.5f);
            L->accMoney = want - give;
            if (give != d) { m = std::min(L->lastMoney + give, 99999); at<int>(udp, G::UD_MONEY) = m; }
        }
        L->lastMoney = m;
    }

    // --- ценность одного солнца: масштабируем всё, что игра добавила сама (сбор шариков) ---
    if (c.sunValOn && fabsf(c.sunVal - 1.f) > 0.01f && !izMode && L->lastSun >= 0 && sun > L->lastSun) {
        int d = sun - L->lastSun;
        if (d <= 500) {
            float want = d * c.sunVal + L->accSunVal;
            int give = (int)floorf(want + 0.5f);
            L->accSunVal = want - give;
            if (give != d) { setSun(board, L->lastSun + give, c); sun = at<int>(board, G::B_SUN); }
        }
    }

    // --- свой потолок солнца ---
    // (то, что уже было сверх потолка, не трогаем: так нельзя потерять солнце, набирая значение в поле; не растёт — и всё)
    if (c.sunCapOn && sun > c.sunCapVal && L->lastSun >= 0 && sun > L->lastSun) { at<int>(board, G::B_SUN) = std::max(c.sunCapVal, L->lastSun); sun = at<int>(board, G::B_SUN); }

    // --- стоимость зомби «Я, зомби»: корректируем списание солнца ---
    if (izMode && c.izCostOn && L->lastSun >= 0 && sun < L->lastSun) {
        int paid = L->lastSun - sun;
        int want = c.izCostEqual ? c.izCostEqualValue : (int)lroundf(paid * c.izCostPct / 100.f);
        setSun(board, L->lastSun - want, c);
        sun = at<int>(board, G::B_SUN);
    }

    // --- выдача солнца ---
    if (InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 0)) { setSun(board, sun + c.sunGive, c); sun = at<int>(board, G::B_SUN); ModSay("Солнце выдано."); }

    // --- волны зомби: пересборка состава ---
    bool wantWaveMod = c.zCountOn || c.zEqual || c.randZombies;
    if (wavesOk && !L->wavesInit && curWave == 0 && numWaves > 0) {
        L->wavesInit = true;
        L->waveSig = !wantWaveMod ? 0 : 1 + (c.zCountOn ? 2 + (int)lroundf(c.zCount * 100.f) * 8 : 0) + (c.zEqual ? 4 : 0) + (c.randZombies ? 8 : 0) * 1000000;
        if (wantWaveMod) rebuildWaves(board, c, 0);
    }
    if (wavesOk && L->wavesInit) {            // настройку включили/выключили/изменили посреди уровня — состав подстраивается сам
        int sig = !wantWaveMod ? 0 : 1 + (c.zCountOn ? 2 + (int)lroundf(c.zCount * 100.f) * 8 : 0) + (c.zEqual ? 4 : 0) + (c.randZombies ? 8 : 0) * 1000000;
        if (sig != L->waveSig) {
            L->waveSig = sig;
            int from = curWave == 0 ? 0 : curWave + 1;                         // уже вышедшие волны не трогаем
            if (wantWaveMod) rebuildWaves(board, c, from); else restoreWaves(board, from);
        }
    }
    if (InterlockedExchange(&S.cmd[CMD_REROLL_WAVES], 0) && wavesOk) { rebuildWaves(board, c, curWave + 1); ModSay("Оставшиеся волны пересобраны."); }

    // --- скорость выхода волн ---
    float waveF = c.zWaveOn ? c.zWave : 1.f;
    if (c.zFirstOn && curWave == 0 && c.zFirst > 0.f) waveF /= c.zFirst;                  // до первой волны: ×2 — ждать вдвое дольше
    if (fabsf(waveF - 1.f) > 0.01f && delta > 0 && wavesOk && curWave < numWaves) {
        L->accWave += (waveF - 1.f) * delta;
        int n = (int)L->accWave; L->accWave -= n;
        int cd = at<int>(board, G::B_ZCOUNTDOWN);
        if (n > 0 && cd > 8) { int nc = cd - n; if (nc < 6) nc = 6; at<int>(board, G::B_ZCOUNTDOWN) = nc; }
        else if (n < 0 && cd > 8 && cd < 20000) at<int>(board, G::B_ZCOUNTDOWN) = cd - n;
    }

    // --- солнце с неба: ускоряем/замедляем обратный отсчёт до следующего падающего солнца ---
    if (c.skySunOn && fabsf(c.skySun - 1.f) > 0.01f && delta > 0 && wavesOk) {
        int sc = at<int>(board, G::B_SCENE);
        if (sc == 0 || sc == 2 || sc == 4) {                 // дневные сцены
            L->accSky += (c.skySun - 1.f) * delta;
            int n = (int)L->accSky; L->accSky -= n;
            int cd = at<int>(board, G::B_SUNCD);
            if (n > 0 && cd > 1)               at<int>(board, G::B_SUNCD) = std::max(1, cd - n);
            else if (n < 0 && cd > 1 && cd < 4000) at<int>(board, G::B_SUNCD) = cd - n;
        }
    }

    // --- растения ---
    char* plants = *(char**)(board + G::B_PLANT);
    int   pmax = at<int>(board, G::B_PLANT_MAX);
    bool  plantsOk = pmax > 0 && pmax <= 2048 && okp(plants, (size_t)pmax * G::PL_SIZE);
    int atkStep[53];                                        // на сколько тактов в этот тик сдвинуть откат выстрела растения данного типа
    int chompN = 0, mineN = 0;                              // скорость пережёвывания чомпера и взвода мины (общий сдвиг счётчика за тик)
    if (c.chomperSpeedOn && !c.chomperInstant && fabsf(c.chomperSpeed - 1.f) > 0.01f && delta > 0) { L->accChomp += (c.chomperSpeed - 1.f) * delta; chompN = (int)L->accChomp; L->accChomp -= chompN; }
    if (c.mineSpeedOn && !c.mineInstant && fabsf(c.mineSpeed - 1.f) > 0.01f && delta > 0) { L->accMine += (c.mineSpeed - 1.f) * delta; mineN = (int)L->accMine; L->accMine -= mineN; }
    for (int t = 0; t < 53; t++) {
        float f = c.atkOn ? c.atkSpeed : 1.f;
        if (c.plantTblOn && t < 48) f *= c.plAtk[t] / 100.f;
        atkStep[t] = 0;
        if (fabsf(f - 1.f) > 0.01f && delta > 0) { L->accAtkT[t] += (f - 1.f) * delta; atkStep[t] = (int)L->accAtkT[t]; L->accAtkT[t] -= atkStep[t]; }
    }
    int sunN = 0;                                           // подсолнухи, солнце-грибы, двойные подсолнухи
    if (c.plantSunOn && fabsf(c.plantSun - 1.f) > 0.01f && delta > 0) {
        L->accSunPl += (c.plantSun - 1.f) * delta; sunN = (int)L->accSunPl; L->accSunPl -= sunN;
    }
    bool hpExisting = InterlockedExchange(&S.cmd[CMD_HP_EXISTING], 0) != 0;
    int sunRef = L->lastSun >= 0 ? L->lastSun : at<int>(board, G::B_SUN);     // солнце до посадки в этом тике
    if (plantsOk) {
        for (int i = 0; i < pmax; i++) {
            char* p = plants + i * G::PL_SIZE;
            bool alive = !at<uint8_t>(p, G::PL_DEAD) && !at<uint8_t>(p, G::PL_SQUISH);
            int type = at<int>(p, G::PL_TYPE);
            if (!alive) { L->hpDone[i] = false; L->prevP[i] = false; continue; }
            if (L->baselined && !L->prevP[i]) {         // новое растение
                int imit = at<int>(p, G::PL_IMIT);
                int cost = plantCost(type, imit);
                if (c.hpOn && c.hpDelta != 0 && !L->hpDone[i]) {
                    int hp = std::max(1, at<int>(p, G::PL_HP) + c.hpDelta);
                    int mx = std::max(1, at<int>(p, G::PL_HPMAX) + c.hpDelta);
                    at<int>(p, G::PL_HP) = hp; at<int>(p, G::PL_HPMAX) = mx; L->hpDone[i] = true;
                }
                int ep = effPlant(type, imit);
                if (c.plantTblOn && ep >= 0 && ep < 48 && c.plHp[ep] != 100) {
                    float k = c.plHp[ep] / 100.f;
                    at<int>(p, G::PL_HP)    = std::max(1, (int)lroundf(at<int>(p, G::PL_HP) * k));
                    at<int>(p, G::PL_HPMAX) = std::max(1, (int)lroundf(at<int>(p, G::PL_HPMAX) * k));
                }
                int s2 = at<int>(board, G::B_SUN);
                if (c.premiumNoBase && !c.freePlants && !izMode && !L->convLevel() && cost > 0) {
                    // Флаг «лёгкой посадки» отключает в игре и проверку, и списание солнца. Возвращаем и то и другое.
                    int took = std::max(0, sunRef - s2);              // если игра всё же списала — второй раз не берём
                    bool sunMode = S.gameMode >= 0 && S.gameMode <= 10;   // приключение и выживание: там солнце настоящее
                    if (sunMode && took == 0 && std::max(sunRef, s2) < cost) {
                        at<uint8_t>(p, G::PL_DEAD) = 1; L->prevP[i] = false; L->hpDone[i] = false;     // не хватило солнца: посадка отменяется
                        ModSay("Не хватает солнца для этого растения.");
                        continue;
                    }
                    s2 -= std::max(0, cost - took);
                }
                if (!c.freePlants && !izMode && !L->convLevel() && cost > 0 && (c.costOn || c.plantTblOn)) {
                    // своя цена: игра списала обычную, мод доплачивает или возвращает разницу
                    float m = (c.costOn ? c.costPct / 100.f : 1.f) * ((c.plantTblOn && ep >= 0 && ep < 48) ? c.plCost[ep] / 100.f : 1.f);
                    int want = (int)lroundf(cost * m), diff = cost - want;
                    if (diff != 0) {
                        int took = std::max(0, sunRef - at<int>(board, G::B_SUN));
                        bool sunMode = S.gameMode >= 0 && S.gameMode <= 10;
                        if (diff < 0 && sunMode && s2 + diff < 0 && !c.premiumNoBase) {        // дороже обычного, а солнца не хватает: посадка отменяется
                            at<uint8_t>(p, G::PL_DEAD) = 1; L->prevP[i] = false; L->hpDone[i] = false;
                            s2 += took; if (s2 != at<int>(board, G::B_SUN)) setSun(board, s2, c);
                            sunRef = at<int>(board, G::B_SUN);
                            ModSay("Не хватает солнца: у этого растения своя цена.");
                            continue;
                        }
                        s2 += diff;
                    }
                }
                if (c.sunBonusOn && (c.sunBonusSeed < 0 || c.sunBonusSeed == type)) s2 += (int)lroundf(cost * c.sunBonusPct / 100.f);
                if (c.premiumBonusOn && isUpgrade(type)) s2 += (int)lroundf(cost * c.premiumBonusPct / 100.f);
                if (s2 != at<int>(board, G::B_SUN)) setSun(board, s2, c);
                sunRef = at<int>(board, G::B_SUN);
            }
            L->prevP[i] = true;
            if (hpExisting && c.hpDelta != 0 && !L->hpDone[i]) {
                at<int>(p, G::PL_HP)    = std::max(1, at<int>(p, G::PL_HP) + c.hpDelta);
                at<int>(p, G::PL_HPMAX) = std::max(1, at<int>(p, G::PL_HPMAX) + c.hpDelta);
                L->hpDone[i] = true;
            }
            if (delta <= 0) continue;
            bool asleep = at<uint8_t>(p, G::PL_ASLEEP) != 0;
            int atkN = (type >= 0 && type < 53) ? atkStep[type] : 0;
            if (atkN != 0 && !asleep && isShooter(type)) {
                int cd = at<int>(p, G::PL_LAUNCHCD);
                if (cd > 0) { int nc = cd - atkN; if (atkN > 0 && nc < 1) nc = 1; at<int>(p, G::PL_LAUNCHCD) = nc; }
            }
            if (sunN != 0 && !asleep && (type == 1 || type == 9 || type == 41)) {
                int cd = at<int>(p, G::PL_LAUNCHCD);
                if (sunN > 0 && cd > 1) at<int>(p, G::PL_LAUNCHCD) = std::max(1, cd - sunN);
                else if (sunN < 0 && cd > 1 && cd < 3000) at<int>(p, G::PL_LAUNCHCD) = cd - sunN;
            }
            int state = at<int>(p, G::PL_STATE);
            if (c.chomperInstant && type == 6 && state == G::PS_CHOMPER_DIGESTING && at<int>(p, G::PL_STATECD) > 1)
                at<int>(p, G::PL_STATECD) = 1;
            if (c.mineInstant && type == 4 && state == G::PS_NOTREADY && at<int>(p, G::PL_STATECD) > 1)
                at<int>(p, G::PL_STATECD) = 1;
            if (chompN != 0 && type == 6 && state == G::PS_CHOMPER_DIGESTING) {
                int cd = at<int>(p, G::PL_STATECD);
                if (chompN > 0 && cd > 1) at<int>(p, G::PL_STATECD) = std::max(1, cd - chompN);
                else if (chompN < 0 && cd > 1 && cd < 6000) at<int>(p, G::PL_STATECD) = cd - chompN;
            }
            if (mineN != 0 && type == 4 && state == G::PS_NOTREADY) {
                int cd = at<int>(p, G::PL_STATECD);
                if (mineN > 0 && cd > 1) at<int>(p, G::PL_STATECD) = std::max(1, cd - mineN);
                else if (mineN < 0 && cd > 1 && cd < 6000) at<int>(p, G::PL_STATECD) = cd - mineN;
            }
        }
    }
    if (hpExisting) ModSay("Здоровье применено к посаженным растениям.");

    // --- зомби ---
    char* zs = *(char**)(board + G::B_ZOMBIE);
    int   zmax = at<int>(board, G::B_ZOMBIE_MAX);
    bool  zOk = zmax > 0 && zmax <= 2048 && okp(zs, (size_t)zmax * G::ZB_SIZE);
    bool  kill = InterlockedExchange(&S.cmd[CMD_KILL_ZOMBIES], 0) != 0;
    int   zBonus = 0;                                       // солнце, пришедшее с убитых зомби в этот тик
    bool  doSpeed = delta > 0 && ((c.zSpeedOn && fabsf(c.zSpeed - 1.f) > 0.01f) || c.zombTblOn);
    if (zOk) {
        for (int i = 0; i < zmax; i++) {
            char* z = zs + i * G::ZB_SIZE;
            bool alive = !at<uint8_t>(z, G::ZB_DEAD);
            if (!alive) {
                if (L->prevZ[i] && c.zSunOn && !izMode && !kill && !L->noBonus[i] && ui == 3) {      // зомби только что погиб
                    int zt = at<int>(z, G::ZB_TYPE); float lx = L->lastZX[i];
                    if (zt != 19 && zt != 20 && lx > -60.f && lx < 860.f && rndN(100) < c.zSunChance) zBonus += c.zSunAmount;
                }
                L->prevZ[i] = false; L->noBonus[i] = false; continue;
            }
            float& px = at<float>(z, G::ZB_POSX);
            if (L->baselined && !L->prevZ[i]) {          // новый зомби
                int fw = at<int>(z, G::ZB_FROMWAVE);
                int zt0 = at<int>(z, G::ZB_TYPE);
                bool zt = c.zombTblOn && zt0 >= 0 && zt0 < 33;
                if ((c.zHpOn || zt) && fw >= -1) {
                    float pBody = (c.zHpOn ? (float)c.zHpPct : 100.f) * (zt ? c.zBody[zt0] : 100) / 100.f;
                    float pArm  = (c.zHpOn ? (float)c.zArmorPct : 100.f) * (zt ? c.zArm[zt0] : 100) / 100.f;
                    auto scale = [&](int off, int offMax, float pct) {
                        int mx = at<int>(z, offMax);
                        if (mx <= 0) return;
                        int nm = std::max(1, (int)lroundf(mx * pct / 100.f));
                        int cur = at<int>(z, off);
                        at<int>(z, offMax) = nm;
                        at<int>(z, off) = std::max(1, (int)lroundf(cur * pct / 100.f));
                    };
                    scale(G::ZB_BODY, G::ZB_BODYMAX, pBody);
                    scale(G::ZB_HELM, G::ZB_HELMMAX, pArm);
                    scale(G::ZB_SHIELD, G::ZB_SHIELDMAX, pArm);
                }
                L->lastZX[i] = px;
            }
            L->prevZ[i] = true;
            if (kill) { at<int>(z, G::ZB_PHASE) = 3; L->noBonus[i] = true; }       // «сбит газонокосилкой» — штатная смерть, солнца за такого зомби нет
            if (doSpeed) {
                int zt1 = at<int>(z, G::ZB_TYPE);
                float f = (c.zSpeedOn ? c.zSpeed : 1.f) * ((c.zombTblOn && zt1 >= 0 && zt1 < 33) ? c.zSpd[zt1] / 100.f : 1.f);
                float dx = L->lastZX[i] - px;             // сколько зомби прошёл за кадр сам
                if (fabsf(f - 1.f) > 0.01f && fabsf(dx) > 0.001f && fabsf(dx) < 6.f * delta) px -= dx * (f - 1.f);
            }
            if (delta > 0 || L->lastZX[i] == 0.f) L->lastZX[i] = px;
        }
    }
    if (zBonus > 0) { setSun(board, at<int>(board, G::B_SUN) + zBonus, c); S.zSunTotal += zBonus; }
    if (kill) ModSay("Зомби уничтожены.");

    // --- семена: перезарядка, случайная замена ---
    char* bank = *(char**)(board + G::B_SEEDBANK);
    if (ui == 3 && okp(bank, 0x350)) {
        int n = at<int>(bank, G::SB_COUNT);
        if (n < 0 || n > 10) n = 0;
        int seedStep[49];                                // сдвиг перезарядки семени данного типа за этот тик (индекс 48 — всё остальное)
        for (int t = 0; t < 49; t++) {
            float f = (c.seedSpeedOn && !c.seedInstant) ? c.seedSpeed : 1.f;
            if (c.plantTblOn && !c.seedInstant && t < 48) f *= c.plSeed[t] / 100.f;
            seedStep[t] = 0;
            if (fabsf(f - 1.f) > 0.01f && delta > 0) { L->accSeedT[t] += (f - 1.f) * delta; seedStep[t] = (int)L->accSeedT[t]; L->accSeedT[t] -= seedStep[t]; }
        }
        if (!L->bankInit) {                               // первый бой-тик: запоминаем счётчики использования
            for (int i = 0; i < n; i++) L->usedPk[i] = at<int>(bank, G::SB_PACKETS + i * G::SP_SIZE + G::SP_USED);
            L->bankInit = true;
            if (g_pendingRandomSet) { g_pendingRandomSet = false; applyRandomSet(board, c); }
        }
        // --- конвейер: уровни, где семена едут по ленте (число семян на ленте меняется, счётчик подачи ненулевой) ---
        {
            int ctr = at<int>(bank, G::SB_CONVEYOR);
            bool belt = false;
            if (L->bankInit && L->bankN >= 0 && n != L->bankN) {
                L->convSeen = true; belt = true;                                        // семена на ленте сдвинулись: пересчитываем счётчики использования
                for (int i = 0; i < n; i++) L->usedPk[i] = at<int>(bank, G::SB_PACKETS + i * G::SP_SIZE + G::SP_USED);
            }
            if (L->bankN >= 0 && n == L->bankN + 1 && L->bankInit && n >= 1) {          // на ленту легло новое семя
                if (c.convSetOn && c.convMask() != 0) {
                    std::vector<int> pool; unsigned long long m = c.convMask();
                    for (int t = 0; t < 48; t++) if ((m >> t) & 1ull) pool.push_back(t);
                    char* pk = bank + G::SB_PACKETS + (n - 1) * G::SP_SIZE;
                    if (!pool.empty()) {
                        int pick;
                        if (c.convPick == 1) pick = pool[(unsigned)L->convCycle++ % pool.size()];          // по кругу, в порядке списка растений
                        else {
                            pick = pool[rndN((int)pool.size())];
                            for (int tries = 0; c.convNoRepeat && pool.size() > 1 && pick == L->convLast && tries < 8; tries++) pick = pool[rndN((int)pool.size())];
                        }
                        L->convLast = pick;
                        at<int>(pk, G::SP_TYPE) = pick; at<int>(pk, G::SP_IMIT) = -1;
                    }
                }
            }
            (void)belt;
            if (L->convCtr >= 0 && delta > 0) {                                           // в какую сторону бежит счётчик
                int d = ctr - L->convCtr;
                if (d < 0 && d >= -40) L->convDown++; else if (d > 0 && d <= 40) L->convUp++;
            }
            L->convCtr = ctr;
            bool downCounter = L->convDown >= 5 && L->convDown > 3 * L->convUp;
            float cs = c.convPause ? 0.f : (c.convSpeedOn ? c.convSpeed : 1.f);       // 0 — лента стоит
            if (fabsf(cs - 1.f) > 0.01f && delta > 0 && downCounter && ctr > 0) {
                L->accConv += (cs - 1.f) * delta;
                int k = (int)L->accConv; L->accConv -= k;
                if (k > 0 && ctr > 1)             { ctr = std::max(1, ctr - k); at<int>(bank, G::SB_CONVEYOR) = ctr; L->convCtr = ctr; }
                else if (k < 0 && ctr < 4000)     { ctr = ctr - k;              at<int>(bank, G::SB_CONVEYOR) = ctr; L->convCtr = ctr; }
            }
            L->bankN = n;
        }
        for (int i = 0; i < n; i++) {
            char* pk = bank + G::SB_PACKETS + i * G::SP_SIZE;
            int ept = effPlant(at<int>(pk, G::SP_TYPE), at<int>(pk, G::SP_IMIT));
            int seedN = seedStep[ept >= 0 && ept < 48 ? ept : 48];
            if (c.seedInstant && at<uint8_t>(pk, G::SP_REFRESHING) && !at<uint8_t>(pk, G::SP_ACTIVE)) {
                at<int>(pk, G::SP_CD) = 0; at<uint8_t>(pk, G::SP_REFRESHING) = 0; at<uint8_t>(pk, G::SP_ACTIVE) = 1;
            } else if (seedN != 0 && at<uint8_t>(pk, G::SP_REFRESHING) && !at<uint8_t>(pk, G::SP_ACTIVE)) {
                int v = at<int>(pk, G::SP_CD) + seedN;
                at<int>(pk, G::SP_CD) = v < 0 ? 0 : v;
            }
            int used = at<int>(pk, G::SP_USED);
            if (used != L->usedPk[i]) {
                if (used > L->usedPk[i] && c.randPlants && !L->convSeen) {
                    std::vector<int> cand; candidatePlants(c, bank, n, i, cand);
                    if (!cand.empty()) { at<int>(pk, G::SP_TYPE) = cand[rndN((int)cand.size())]; at<int>(pk, G::SP_IMIT) = -1; }
                }
                L->usedPk[i] = used;
            }
        }
    }
    if (InterlockedExchange(&S.cmd[CMD_PLANTS_SLOTS], 0)) { if (ui == 3) putPlantsInSlots(board, plantMaskNow()); else ModSay("Слоты выдаются, когда бой начался."); }
    if (InterlockedExchange(&S.cmd[CMD_RANDOM_SET], 0)) { if (ui == 3) applyRandomSet(board, c); else ModSay("Набор выдаётся, когда бой начался."); }

    // --- события после каждой волны ---
    if (wavesOk && ui == 3) {
        if (L->lastWave < 0) L->lastWave = curWave;
        if (curWave > L->lastWave) {
            L->lastWave = curWave;
            if (c.evGraves && rndN(100) < c.evChance) evGraves(board, c);
            if (c.evFog && S.scene == 3 && rndN(100) < c.evChance) { at<int>(board, G::B_FOGBLOWN) = 2000; ModSay("Событие: ветер сдул туман."); }
            if (c.evDayNight && rndN(100) < c.evChance) evDayNight(board, c);
        }
    }

    L->baselined = true;
    L->lastSun = at<int>(board, G::B_SUN);
}

#ifdef PVZ_TEST
void testTick() { tick(); }
#else
// ============================================================================
//  Хуки таблицы импорта: вызываем tick() в игровом потоке
// ============================================================================
static bool g_inTick = false;
static void onHook() {
    if (!g_ready || g_inTick || GetCurrentThreadId() != g_mainTid) return;
    static LONGLONG lastQpc = 0; static LARGE_INTEGER f = {};
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    if (n.QuadPart - lastQpc < f.QuadPart / 1000) return;     // не чаще раза в миллисекунду
    lastQpc = n.QuadPart;
    g_inTick = true;
    tick();
    g_inTick = false;
}
static VOID (WINAPI* oSleep)(DWORD);
static BOOL (WINAPI* oPeekA)(LPMSG, HWND, UINT, UINT, UINT);
static BOOL (WINAPI* oPeekW)(LPMSG, HWND, UINT, UINT, UINT);
static VOID WINAPI hkSleep(DWORD ms) { onHook(); oSleep(ms); }
static BOOL WINAPI hkPeekA(LPMSG m, HWND h, UINT a, UINT b, UINT r) { onHook(); return oPeekA(m, h, a, b, r); }
static BOOL WINAPI hkPeekW(LPMSG m, HWND h, UINT a, UINT b, UINT r) { onHook(); return oPeekW(m, h, a, b, r); }

static bool hookIAT(HMODULE mod, const char* dll, const char* fn, void* hook, void** orig) {
    auto base = (uint8_t*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return false;
    for (auto d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; d++) {
        if (_stricmp((const char*)(base + d->Name), dll) != 0) continue;
        auto thunk = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        auto names = (IMAGE_THUNK_DATA*)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (; names->u1.AddressOfData; names++, thunk++) {
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            auto ibn = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, fn) != 0) continue;
            DWORD old; VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            *orig = (void*)thunk->u1.Function;
            thunk->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    return false;
}

// ============================================================================
//  Инициализация
// ============================================================================
static void ensureDir(const std::wstring& p) { CreateDirectoryW(p.c_str(), nullptr); }

static void backupUserdata(const std::wstring& exeDir) {
    std::wstring src = exeDir + L"\\userdata", dstRoot = g_dataDir + L"\\backup", dst = dstRoot + L"\\userdata_first_run";
    if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) return;     // копия уже есть
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    ensureDir(dstRoot); ensureDir(dst);
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((src + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        CopyFileW((src + L"\\" + fd.cFileName).c_str(), (dst + L"\\" + fd.cFileName).c_str(), TRUE);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static std::string readFileUtf8(const std::wstring& p) {
    std::string out; HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    char buf[4096]; DWORD n;
    while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n) out.append(buf, n);
    CloseHandle(h); return out;
}

static DWORD WINAPI initThread(LPVOID) {
    InitializeCriticalSection(&S.lock);
    S.cfg = Cfg();
    GetTickCount(); g_rng ^= GetTickCount() * 2654435761u; if (!g_rng) g_rng = 1;

    wchar_t path[MAX_PATH]; HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&initThread, &self);
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring dllDir = path; dllDir = dllDir.substr(0, dllDir.find_last_of(L'\\'));
    g_dataDir = dllDir + L"\\PvZMod"; ensureDir(g_dataDir); ensureDir(g_dataDir + L"\\profiles");
    cfgFromIni(S.cfg, readFileUtf8(g_dataDir + L"\\last.ini"));
    runsFromIni(S.runs, readFileUtf8(g_dataDir + L"\\runs.ini"));

    for (int i = 0; i < 600 && !g_gameWnd; i++) {            // ждём главное окно игры до 60 с
        HWND w = findOwnGameWindow();
        if (w) { g_gameWnd = w; g_mainTid = GetWindowThreadProcessId(w, nullptr); }
        else Sleep(100);
    }
    auto base = (uint8_t*)GetModuleHandleW(nullptr);
    DWORD stamp = *(DWORD*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew + 8);
    S.versionOk = (g_gameWnd && (uintptr_t)base == 0x400000 && stamp == G::TIMESTAMP) ? 1 : 0;
    if (S.versionOk) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring exeDir = exe; exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\'));
        backupUserdata(exeDir);
        buildHacks();
        HMODULE exeMod = GetModuleHandleW(nullptr);
        bool ok = hookIAT(exeMod, "KERNEL32.dll", "Sleep", (void*)hkSleep, (void**)&oSleep);
        ok |= hookIAT(exeMod, "USER32.dll", "PeekMessageA", (void*)hkPeekA, (void**)&oPeekA);
        ok |= hookIAT(exeMod, "USER32.dll", "PeekMessageW", (void*)hkPeekW, (void**)&oPeekW);
        S.hooked = ok ? 1 : 0;
        g_ready = true;
        ModSay(ok ? "Мод подключён. Меню — клавиша Tab." : "Не удалось подключиться к игровому циклу.");
    } else {
        ModSay("Эта сборка игры не поддерживается (нужна Steam GOTY 1.2.0.1096). Функции мода отключены.");
    }
    UiStart();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        CreateThread(nullptr, 0, initThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

#endif  // PVZ_TEST
