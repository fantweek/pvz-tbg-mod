// Тест логики мода на подделанной памяти игры (32-битный Linux). Проверяет внутреннюю согласованность
// и отсутствие выходов за границы; реальные смещения игры этим тестом не проверяются.
#define PVZ_TEST 1
#include "mod.cpp"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { fails++; printf("FAIL  %s  (line %d)\n", msg, __LINE__); } } while (0)

struct World {
    char* app; char* board; char* plants; char* zombies; char* bank; char* chall; char* ud;
    World() {
        app = (char*)calloc(1, 0x1000); board = (char*)calloc(1, 0x5700); plants = (char*)calloc(64, G::PL_SIZE);
        zombies = (char*)calloc(64, G::ZB_SIZE); bank = (char*)calloc(1, 0x350); chall = (char*)calloc(1, 0x200);
        ud = (char*)calloc(1, 0x400);
        at<int>(app, G::APP_UI) = 3; at<int>(app, G::APP_FRAME_MS) = 10;
        *(char**)(app + G::APP_BOARD) = board; *(char**)(app + G::APP_USERDATA) = ud;
        *(char**)(board + G::B_PLANT) = plants; at<int>(board, G::B_PLANT_MAX) = 64;
        *(char**)(board + G::B_ZOMBIE) = zombies; at<int>(board, G::B_ZOMBIE_MAX) = 64;
        *(char**)(board + G::B_SEEDBANK) = bank; *(char**)(board + G::B_CHALLENGE) = chall;
        at<int>(board, G::B_SUN) = 500; at<int>(board, G::B_NUMWAVES) = 10; at<int>(board, G::B_ZCOUNTDOWN) = 1000;
        at<int>(bank, G::SB_COUNT) = 6;
        for (int i = 0; i < 6; i++) { char* pk = bank + G::SB_PACKETS + i * G::SP_SIZE; at<int>(pk, G::SP_TYPE) = i; at<uint8_t>(pk, G::SP_ACTIVE) = 1; }
        for (int i = 0; i < 64; i++) { at<uint8_t>(plants + i * G::PL_SIZE, G::PL_DEAD) = 1; at<uint8_t>(zombies + i * G::ZB_SIZE, G::ZB_DEAD) = 1; }
        int* list = (int*)(board + G::B_SPAWN_LIST);
        for (int w = 0; w < 100; w++) list[w * 50] = -1;
        for (int w = 0; w < 10; w++) {
            int k = 0;
            if (w == 4 || w == 9) { for (int i = 0; i < 4; i++) list[w * 50 + k++] = 0; list[w * 50 + k++] = 1; }
            for (int i = 0; i < 3; i++) list[w * 50 + k++] = 0;
            list[w * 50 + k++] = 2;
            list[w * 50 + k++] = 4;
            list[w * 50 + k] = -1;
        }
        g_testApp = app;
    }
    int& sun() { return at<int>(board, G::B_SUN); }
    int& clock() { return at<int>(board, G::B_CLOCK); }
    char* plant(int i) { return plants + i * G::PL_SIZE; }
    char* zomb(int i) { return zombies + i * G::ZB_SIZE; }
    char* pk(int i) { return bank + G::SB_PACKETS + i * G::SP_SIZE; }
    void step(int n = 1) { for (int i = 0; i < n; i++) { clock()++; testTick(); } }
    void addPlant(int i, int type, int hp = 300) {
        char* p = plant(i); at<uint8_t>(p, G::PL_DEAD) = 0; at<uint8_t>(p, G::PL_SQUISH) = 0;
        at<int>(p, G::PL_TYPE) = type; at<int>(p, G::PL_HP) = hp; at<int>(p, G::PL_HPMAX) = hp;
    }
    void addZombie(int i, int type, float x, int body, int helm = 0, int shield = 0) {
        char* z = zomb(i); at<uint8_t>(z, G::ZB_DEAD) = 0; at<int>(z, G::ZB_TYPE) = type; at<float>(z, G::ZB_POSX) = x;
        at<int>(z, G::ZB_BODY) = body; at<int>(z, G::ZB_BODYMAX) = body;
        at<int>(z, G::ZB_HELM) = helm; at<int>(z, G::ZB_HELMMAX) = helm;
        at<int>(z, G::ZB_SHIELD) = shield; at<int>(z, G::ZB_SHIELDMAX) = shield; at<int>(z, G::ZB_FROMWAVE) = 0;
    }
};

static void setCfg(const Cfg& c) { S.cfg = c; }
static int waveLen(World& w, int wave) { int* l = (int*)(w.board + G::B_SPAWN_LIST) + wave * 50; int n = 0; while (n < 50 && l[n] >= 0) n++; return n; }

static void setPlayerName(char* ud, const char* n) {      // MSVC std::string: буфер +4 (до 15 символов), размер +0x14, ёмкость +0x18
    int len = (int)strlen(n); if (len > 15) len = 15;
    memset(ud + G::UD_NAME, 0, 0x1c); memcpy(ud + G::UD_NAME + 4, n, len);
    at<int>(ud, G::UD_NAME + 0x14) = len; at<int>(ud, G::UD_NAME + 0x18) = 15;
}

int main() {
    InitializeCriticalSection(&S.lock); S.cfg = Cfg();

    { // --- 1. всё выключено: ничего не меняется
        World w; Cfg c; setCfg(c); w.step(3);
        CHECK(w.sun() == 500, "off: sun unchanged");
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 0, "off: no free planting flag");
        CHECK(waveLen(w, 0) == 5, "off: wave list untouched");
    }
    { // --- 2. бесплатные растения: флаг включается и выключается
        World w; Cfg c; c.freePlants = true; setCfg(c); w.step(2);
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 1, "free: flag on");
        c.freePlants = false; setCfg(c); w.step(1);
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 0, "free: flag restored");
    }
    { // --- 3. бонус солнц + HP для новых растений
        World w; Cfg c; c.sunBonusOn = true; c.sunBonusPct = 100; c.hpOn = true; c.hpDelta = -100; setCfg(c); w.step(2);
        w.addPlant(3, 0); w.step(1);       // горохострел, цена 100
        CHECK(w.sun() == 600, "bonus: +100% of cost 100");
        CHECK(at<int>(w.plant(3), G::PL_HP) == 200 && at<int>(w.plant(3), G::PL_HPMAX) == 200, "hp -100 applied");
        w.step(3); CHECK(w.sun() == 600, "bonus: not repeated");
        c.sunBonusSeed = 1; setCfg(c); w.addPlant(4, 0); w.step(1);
        CHECK(w.sun() == 600, "bonus: specific seed only (peashooter skipped)");
        w.addPlant(5, 1); w.step(1);
        CHECK(w.sun() == 650, "bonus: sunflower 50");
    }
    { // --- 4. премиум без носителя без «бесплатных»: списываем цену сами
        World w; Cfg c; c.premiumNoBase = true; c.premiumBonusOn = true; c.premiumBonusPct = 50; setCfg(c); w.step(2);
        w.addPlant(0, 40);                 // гатлинг, цена 250
        w.step(1); CHECK(w.sun() == 500 - 250 + 125, "premium: charge cost, bonus 50%");
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 1, "premium: flag on");
    }
    { // --- 5. скорость атаки ×2 / ×0.5
        World w; Cfg c; c.atkOn = true; c.atkSpeed = 2.f; setCfg(c); w.step(2);
        w.addPlant(0, 0); at<int>(w.plant(0), G::PL_LAUNCHCD) = 100; w.step(1);   // посадка
        int a = at<int>(w.plant(0), G::PL_LAUNCHCD); w.step(10);
        int b = at<int>(w.plant(0), G::PL_LAUNCHCD);
        CHECK(a - b == 10 * 1, "atk x2: 10 ticks remove 10 extra (game removes the other 10)");
        c.atkSpeed = 0.5f; setCfg(c); at<int>(w.plant(0), G::PL_LAUNCHCD) = 100; w.step(20);
        CHECK(at<int>(w.plant(0), G::PL_LAUNCHCD) == 110, "atk x0.5: 20 ticks add 10");
        c.atkSpeed = 8.f; setCfg(c); at<int>(w.plant(0), G::PL_LAUNCHCD) = 5; w.step(1);
        CHECK(at<int>(w.plant(0), G::PL_LAUNCHCD) >= 1, "atk x8: counter never skipped below 1");
        at<uint8_t>(w.plant(0), G::PL_ASLEEP) = 1; at<int>(w.plant(0), G::PL_LAUNCHCD) = 100; w.step(5);
        CHECK(at<int>(w.plant(0), G::PL_LAUNCHCD) == 100, "atk: sleeping plant untouched");
    }
    { // --- 6. чомпер и мина
        World w; Cfg c; c.chomperInstant = true; c.mineInstant = true; setCfg(c); w.step(2);
        w.addPlant(0, 6); at<int>(w.plant(0), G::PL_STATE) = G::PS_CHOMPER_DIGESTING; at<int>(w.plant(0), G::PL_STATECD) = 4000;
        w.addPlant(1, 4); at<int>(w.plant(1), G::PL_STATE) = G::PS_NOTREADY; at<int>(w.plant(1), G::PL_STATECD) = 1500;
        w.step(2);
        CHECK(at<int>(w.plant(0), G::PL_STATECD) == 1, "chomper countdown -> 1");
        CHECK(at<int>(w.plant(1), G::PL_STATECD) == 1, "mine countdown -> 1");
    }
    { // --- 6c. счётчик «что включено» и сброс по вкладкам
        Cfg c;
        CHECK(cfgActiveTotal(c) == 0 && !cfgGroupChanged(c, GRP_PLANTS) && !cfgGroupChanged(c, GRP_ZOMBIES) && !cfgGroupChanged(c, GRP_LEVEL), "defaults: nothing active, nothing changed");
        c.atkOn = true; c.freePlants = true; c.zCountOn = true; c.evFog = true; c.gameSpeedOn = true; c.plantTblOn = true; c.plHp[3] = 300;
        CHECK(cfgActiveCount(c, GRP_PLANTS) == 3 && cfgActiveCount(c, GRP_ZOMBIES) == 1 && cfgActiveCount(c, GRP_LEVEL) == 2, "active counts per tab");
        CHECK(cfgActiveTotal(c) == 6, "active total is the sum of tabs");
        c.ulStore = true; c.moneyGive = 5; c.startLevel = 7; c.bgRun = false;
        CHECK(cfgActiveTotal(c) == 6, "tools and system flags are not counted");
        Cfg r = c; cfgResetGroup(r, GRP_PLANTS);
        CHECK(!r.atkOn && !r.freePlants && !r.plantTblOn && r.plHp[3] == 100 && r.zCountOn && r.evFog, "reset plants tab: only that tab");
        CHECK(r.ulStore && r.moneyGive == 5 && r.startLevel == 7 && !r.bgRun && r.sunGive == c.sunGive, "reset keeps tools, give-amounts and background run");
        cfgResetGroup(r, GRP_ZOMBIES); cfgResetGroup(r, GRP_LEVEL);
        CHECK(cfgActiveTotal(r) == 0 && !cfgGroupChanged(r, GRP_PLANTS) && !cfgGroupChanged(r, GRP_ZOMBIES) && !cfgGroupChanged(r, GRP_LEVEL), "all tabs reset: back to vanilla settings");
        Cfg sl; sl.atkSpeed = 3.f; CHECK(cfgGroupChanged(sl, GRP_PLANTS) && cfgActiveCount(sl, GRP_PLANTS) == 0, "slider value moved with flag off still counts as changed");
        for (int i = 0; i < kBuildCount; i++) CHECK(cfgActiveTotal(buildCfg(i)) >= 1, "every build changes something");
        Cfg v; presetVanilla(v); CHECK(cfgActiveTotal(v) == 0, "vanilla preset has nothing active");
    }
    { // --- 6b. скорость чомпера и мины (множитель), мгновенный режим главнее
        World w; Cfg c; c.chomperSpeedOn = true; c.chomperSpeed = 3.f; c.mineSpeedOn = true; c.mineSpeed = 0.5f; setCfg(c); w.step(2);
        w.addPlant(0, 6); at<int>(w.plant(0), G::PL_STATE) = G::PS_CHOMPER_DIGESTING; at<int>(w.plant(0), G::PL_STATECD) = 4000;
        w.addPlant(1, 4); at<int>(w.plant(1), G::PL_STATE) = G::PS_NOTREADY; at<int>(w.plant(1), G::PL_STATECD) = 1500;
        w.step(10);
        CHECK(at<int>(w.plant(0), G::PL_STATECD) == 3980, "chomper x3: counter drops 2 extra per tick");
        CHECK(at<int>(w.plant(1), G::PL_STATECD) == 1505, "mine x0.5: counter held back by half a tick per tick");
        c.chomperInstant = true; setCfg(c); w.step(2);
        CHECK(at<int>(w.plant(0), G::PL_STATECD) == 1, "chomper: instant beats the multiplier");
        World w2; Cfg c2; c2.chomperSpeedOn = true; c2.chomperSpeed = 5.f; setCfg(c2); w2.step(2);
        w2.addPlant(0, 6); at<int>(w2.plant(0), G::PL_STATE) = G::PS_CHOMPER_DIGESTING; at<int>(w2.plant(0), G::PL_STATECD) = 3; w2.step(5);
        CHECK(at<int>(w2.plant(0), G::PL_STATECD) == 1, "chomper: never pushed below 1 (the game finishes it itself)");
        World w3; Cfg c3; c3.mineSpeedOn = true; c3.mineSpeed = 4.f; setCfg(c3); w3.step(2);
        w3.addPlant(0, 0); at<int>(w3.plant(0), G::PL_STATE) = G::PS_NOTREADY; at<int>(w3.plant(0), G::PL_STATECD) = 100; w3.step(5);
        CHECK(at<int>(w3.plant(0), G::PL_STATECD) == 100, "mine speed does not touch other plants");
    }
    { // --- 7. семена: без задержек, множитель, случайная замена
        World w; Cfg c; c.seedInstant = true; setCfg(c); w.step(2);
        at<uint8_t>(w.pk(2), G::SP_ACTIVE) = 0; at<uint8_t>(w.pk(2), G::SP_REFRESHING) = 1; at<int>(w.pk(2), G::SP_CD) = 50; w.step(1);
        CHECK(at<uint8_t>(w.pk(2), G::SP_ACTIVE) == 1 && at<uint8_t>(w.pk(2), G::SP_REFRESHING) == 0, "seed instant");
        c = Cfg(); c.seedSpeedOn = true; c.seedSpeed = 3.f; setCfg(c);
        at<uint8_t>(w.pk(2), G::SP_ACTIVE) = 0; at<uint8_t>(w.pk(2), G::SP_REFRESHING) = 1; at<int>(w.pk(2), G::SP_CD) = 0; w.step(10);
        CHECK(at<int>(w.pk(2), G::SP_CD) == 20, "seed x3: 10 ticks add 20 extra");
        c = Cfg(); c.randPlants = true; setCfg(c);
        for (int t = 0; t < 200; t++) {
            int before[6]; for (int i = 0; i < 6; i++) before[i] = at<int>(w.pk(i), G::SP_TYPE);
            int slot = t % 6; at<int>(w.pk(slot), G::SP_USED)++; w.step(1);
            int nt = at<int>(w.pk(slot), G::SP_TYPE); bool dup = false;
            for (int i = 0; i < 6; i++) if (i != slot && at<int>(w.pk(i), G::SP_TYPE) == nt) dup = true;
            if (dup || nt < 0 || nt > 47 || isUpgrade(nt)) { CHECK(false, "random seed: valid, unique, not premium"); break; }
            (void)before;
        }
    }
    { // --- 8. зомби: скорость и здоровье
        World w; Cfg c; c.zSpeedOn = true; c.zSpeed = 2.f; c.zHpOn = true; c.zHpPct = 200; c.zArmorPct = 300; setCfg(c); w.step(2);
        w.addZombie(0, 4, 700.f, 270, 370);
        w.step(1);
        CHECK(at<int>(w.zomb(0), G::ZB_BODY) == 540 && at<int>(w.zomb(0), G::ZB_BODYMAX) == 540, "zombie body x2");
        CHECK(at<int>(w.zomb(0), G::ZB_HELM) == 1110 && at<int>(w.zomb(0), G::ZB_HELMMAX) == 1110, "zombie armor x3");
        CHECK(at<int>(w.zomb(0), G::ZB_SHIELDMAX) == 0, "no shield stays 0");
        float x0 = at<float>(w.zomb(0), G::ZB_POSX);
        for (int i = 0; i < 20; i++) { at<float>(w.zomb(0), G::ZB_POSX) -= 0.25f; w.step(1); }   // игра двигает на 0.25/кадр
        float moved = x0 - at<float>(w.zomb(0), G::ZB_POSX);
        CHECK(fabsf(moved - 20 * 0.5f) < 0.3f, "zombie speed x2: 0.25 -> 0.5 per tick");
        c.zSpeed = 0.5f; setCfg(c); x0 = at<float>(w.zomb(0), G::ZB_POSX);
        for (int i = 0; i < 20; i++) { at<float>(w.zomb(0), G::ZB_POSX) -= 0.25f; w.step(1); }
        moved = x0 - at<float>(w.zomb(0), G::ZB_POSX);
        CHECK(fabsf(moved - 20 * 0.125f) < 0.3f, "zombie speed x0.5");
        at<int>(w.app, G::APP_MODE) = 0;
        InterlockedExchange(&S.cmd[CMD_KILL_ZOMBIES], 1); w.step(1);
        CHECK(at<int>(w.zomb(0), G::ZB_PHASE) == 3, "kill zombies command");
    }
    { // --- 9. состав волн: количество, равные типы, флаги, границы
        World w; Cfg c; c.zCountOn = true; c.zCount = 2.f; setCfg(c); w.step(1);
        CHECK(waveLen(w, 0) == 10, "waves x2: 5 -> 10");
        CHECK(waveLen(w, 4) == 1 + 2 * 9, "waves x2 with flag wave (flag + 9 normals doubled)");
        int* l4 = (int*)(w.board + G::B_SPAWN_LIST) + 4 * 50; CHECK(l4[0] == 1, "flag zombie kept first");
        World w2; Cfg c2; c2.zCountOn = true; c2.zCount = 6.f; setCfg(c2); w2.step(1);
        for (int k = 0; k < 10; k++) CHECK(waveLen(w2, k) <= 49, "waves x6 capped");
        World w3; Cfg c3; c3.zEqual = true; setCfg(c3); w3.step(1);
        int cnt[8] = {}; for (int k = 0; k < 10; k++) { int* l = (int*)(w3.board + G::B_SPAWN_LIST) + k * 50; for (int i = 0; l[i] >= 0 && i < 50; i++) if (l[i] < 8) cnt[l[i]]++; }
        CHECK(cnt[0] > 0 && cnt[2] > 0 && cnt[4] > 0, "equalize: all level types present");
        CHECK(waveLen(w3, 1) == 5, "equalize: count unchanged");
        World w4; Cfg c4; c4.randZombies = true; setCfg(c4); w4.step(1);
        CHECK(waveLen(w4, 2) == 5, "random zombies: count unchanged");
        World w5; Cfg c5; c5.zCountOn = true; c5.zCount = 0.4f; setCfg(c5); w5.step(1);
        CHECK(waveLen(w5, 1) >= 1 && waveLen(w5, 1) <= 3, "waves x0.4 keeps at least one");
        // пересборка посреди боя — только будущие волны
        World w6; Cfg c6; setCfg(c6); w6.step(2); at<int>(w6.board, G::B_CURWAVE) = 3; w6.step(1);
        c6.zCountOn = true; c6.zCount = 2.f; setCfg(c6); InterlockedExchange(&S.cmd[CMD_REROLL_WAVES], 1); w6.step(1);
        CHECK(waveLen(w6, 2) == 5 && waveLen(w6, 3) == 5 && waveLen(w6, 5) == 10, "reroll touches only future waves");
    }
    { // --- 10. таймер волн
        World w; Cfg c; c.zWaveOn = true; c.zWave = 3.f; setCfg(c); w.step(1);
        int a = at<int>(w.board, G::B_ZCOUNTDOWN); w.step(10);
        CHECK(a - at<int>(w.board, G::B_ZCOUNTDOWN) == 20, "wave timer x3: 10 ticks remove 20 extra");
        at<int>(w.board, G::B_ZCOUNTDOWN) = 9; w.step(5);
        CHECK(at<int>(w.board, G::B_ZCOUNTDOWN) >= 6, "wave timer never jumps past 6");
    }
    { // --- 11. события после волн
        World w; Cfg c; c.evGraves = true; c.evDayNight = true; c.evFog = true; c.evChance = 100; c.evGraveCount = 3; setCfg(c); w.step(3);
        w.addPlant(0, 8); at<uint8_t>(w.plant(0), G::PL_ASLEEP) = 0;     // пуф-гриб
        w.step(1); at<int>(w.board, G::B_CURWAVE) = 1; w.step(1);
        CHECK(t_graves == 3, "event: 3 graves");
        CHECK(at<int>(w.board, G::B_SCENE) == 1 && t_bg == 1, "event: day -> night");
        at<int>(w.board, G::B_CURWAVE) = 2; w.step(1);
        CHECK(at<int>(w.board, G::B_SCENE) == 0 && t_sleeps == 1, "event: night -> day, mushroom sleeps");
        at<int>(w.board, G::B_SCENE) = 3; at<int>(w.board, G::B_CURWAVE) = 3; w.step(1);
        CHECK(at<int>(w.board, G::B_FOGBLOWN) == 2000, "event: fog blown");
        at<int>(w.board, G::B_SCENE) = 2; int bg = t_bg; at<int>(w.board, G::B_CURWAVE) = 4; w.step(1);
        CHECK(t_bg == bg, "event: pool scene is not swapped");
    }
    { // --- 12. «Я, зомби»: цена
        World w; Cfg c; c.izCostOn = true; c.izCostPct = 50; setCfg(c);
        at<int>(w.app, G::APP_MODE) = 61; w.step(2);
        w.sun() -= 150; w.step(1);
        CHECK(w.sun() == 500 - 75, "iz: 50% of 150");
        c.izCostEqual = true; c.izCostEqualValue = 20; setCfg(c); w.sun() -= 175; w.step(1);
        CHECK(w.sun() == 500 - 75 - 20, "iz: equal cost 20");
        at<int>(w.app, G::APP_MODE) = 0; w.sun() -= 100; w.step(1);
        CHECK(w.sun() == 500 - 75 - 20 - 100, "iz cost ignored outside iz modes");
    }
    { // --- 13. случайный набор + команды
        World w; Cfg c; c.randSetCount = 6; setCfg(c); w.step(2);
        InterlockedExchange(&S.cmd[CMD_RANDOM_SET], 1); w.step(1);
        bool sunPlant = false, dup = false; int seen[53] = {};
        for (int i = 0; i < 6; i++) { int t = at<int>(w.pk(i), G::SP_TYPE); if (t == 1 || t == 9) sunPlant = true; if (seen[t]++) dup = true; if (t < 0 || t > 47 || isUpgrade(t)) dup = true; }
        CHECK(sunPlant && !dup, "random set: sun plant present, unique");
        c.randKeepSun = false; setCfg(c); InterlockedExchange(&S.cmd[CMD_RANDOM_SET], 1); w.step(1);
        c.sunGive = 1234; setCfg(c); InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 1); int s0 = w.sun(); w.step(1);
        CHECK(w.sun() == s0 + 1234, "give sun");
        c.sunGive = 99990; setCfg(c); InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 1); w.step(1);
        CHECK(w.sun() == 9990, "sun capped at 9990");
        c.noSunCap = true; setCfg(c); InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 1); w.step(1);
        CHECK(w.sun() == 99990, "sun cap lifted");
    }
    { // --- 14. профиль: разблокировка и выдача, main menu
        World w; at<int>(w.app, G::APP_UI) = 1; Cfg c; c.ulStore = c.ulAlmanac = c.ulModes = c.ulBuyAll = true; c.moneyGive = 5000; c.fertGive = 7; setCfg(c);
        InterlockedExchange(&S.cmd[CMD_UNLOCK], 1); InterlockedExchange(&S.cmd[CMD_GIVE_MONEY], 1); InterlockedExchange(&S.cmd[CMD_GIVE_FERT], 1);
        testTick();
        CHECK(at<int>(w.ud, G::UD_UPSELL) == 1 && at<int>(w.ud, G::UD_FINISHED) == 1, "unlock store/almanac flags");
        CHECK(at<int>(w.ud, G::UD_MONEY) == 500, "money 5000 -> 500 coins");
        CHECK(((int*)(w.ud + G::UD_PURCHASES))[14] == 1007, "fertilizer 1000+7");
        CHECK(((int*)(w.ud + G::UD_PURCHASES))[21] == 4 && ((int*)(w.ud + G::UD_PURCHASES))[0] == 1, "buy all");
        CHECK(at<int>(w.ud, G::UD_RECORDS) == 5 && ((int*)(w.ud + G::UD_RECORDS))[15] == 1, "mode records written");
        InterlockedExchange(&S.cmd[CMD_START], 1); c.startLevel = 37; setCfg(c); InterlockedExchange(&S.cmd[CMD_START], 1); testTick();
        CHECK(at<int>(w.ud, G::UD_LEVEL) == 37, "start level writes profile level");
        InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 1); testTick();   // вне боя — команда не должна «зависнуть»
        CHECK(S.cmd[CMD_GIVE_SUN] == 0, "battle-only command is dropped in main menu");
    }
    { // --- 15. граничные случаи: мусорные значения волн/нулевая доска, смена доски
        World w; Cfg c; c.zCountOn = true; c.zCount = 2.f; c.zWaveOn = true; c.zWave = 2.f; setCfg(c);
        at<int>(w.board, G::B_NUMWAVES) = 5000; at<int>(w.board, G::B_CURWAVE) = -4; w.step(3);
        at<int>(w.board, G::B_PLANT_MAX) = 100000; at<int>(w.board, G::B_ZOMBIE_MAX) = -5; w.step(3);
        at<int>(w.board, G::B_SEEDBANK) = 0; *(char**)(w.board + G::B_SEEDBANK) = nullptr; w.step(3);
        World w2; setCfg(c); w2.step(2); w2.clock() = 0; w2.step(2);                 // часы «перемотались» — новый бой
        CHECK(true, "garbage state survived");
    }

    { // --- 16. ценность одного солнца (в обе стороны)
        World w; Cfg c; c.sunValOn = true; c.sunVal = 2.f; setCfg(c); w.step(2);
        w.sun() += 25; w.step(1); CHECK(w.sun() == 550, "sun value x2: 25 -> 50");
        w.sun() -= 100; w.step(1); CHECK(w.sun() == 450, "spending is not scaled");
        c.sunVal = 0.5f; setCfg(c); w.sun() += 25; w.step(1); w.sun() += 25; w.step(1);
        CHECK(w.sun() == 475, "sun value x0.5: two 25s give 25 total (fraction carried)");
        c.sunValOn = false; setCfg(c); w.sun() += 25; w.step(1); CHECK(w.sun() == 500, "off: unchanged");
        World wi; c.sunValOn = true; c.sunVal = 3.f; setCfg(c); at<int>(wi.app, G::APP_MODE) = 61; wi.step(2); wi.sun() += 50; wi.step(1);
        CHECK(wi.sun() == 550, "sun value ignored in I, Zombie");
        World wb; c.sunValOn = true; c.sunVal = 3.f; setCfg(c); wb.step(2); wb.sun() += 9000; wb.step(1);
        CHECK(wb.sun() == 9500, "huge jumps (not sun coins) are not scaled");
    }
    { // --- 17. скорость солнца с неба
        World w; Cfg c; c.skySunOn = true; c.skySun = 2.f; setCfg(c); at<int>(w.board, G::B_SUNCD) = 500; w.step(1);
        int a = at<int>(w.board, G::B_SUNCD); w.step(10);
        CHECK(a - at<int>(w.board, G::B_SUNCD) == 10, "sky sun x2: 10 ticks remove 10 extra");
        c.skySun = 0.5f; setCfg(c); a = at<int>(w.board, G::B_SUNCD); w.step(10);
        CHECK(at<int>(w.board, G::B_SUNCD) - a == 5, "sky sun x0.5: 10 ticks add 5");
        at<int>(w.board, G::B_SUNCD) = 3; c.skySun = 5.f; setCfg(c); w.step(20);
        CHECK(at<int>(w.board, G::B_SUNCD) == 1, "sky sun never reaches 0 on its own");
        World wn; c.skySun = 2.f; setCfg(c); at<int>(wn.board, G::B_SCENE) = 1; at<int>(wn.board, G::B_SUNCD) = 500; wn.step(11);
        CHECK(at<int>(wn.board, G::B_SUNCD) == 500, "sky sun untouched at night");
    }
    { // --- 18. скорость солнечных растений
        World w; Cfg c; c.plantSunOn = true; c.plantSun = 2.f; setCfg(c);
        w.addPlant(0, 1); w.addPlant(1, 9); w.addPlant(2, 41); w.addPlant(3, 3);
        for (int i = 0; i < 4; i++) at<int>(w.plant(i), G::PL_LAUNCHCD) = 400;
        at<uint8_t>(w.plant(1), G::PL_ASLEEP) = 1;
        w.step(2); int a = at<int>(w.plant(0), G::PL_LAUNCHCD); w.step(10);
        CHECK(a - at<int>(w.plant(0), G::PL_LAUNCHCD) == 10, "sunflower x2 faster");
        CHECK(at<int>(w.plant(2), G::PL_LAUNCHCD) < 395, "twin sunflower faster");
        CHECK(at<int>(w.plant(1), G::PL_LAUNCHCD) == 400, "sleeping sun-shroom untouched");
        CHECK(at<int>(w.plant(3), G::PL_LAUNCHCD) == 400, "wall-nut untouched");
        c.plantSun = 0.5f; setCfg(c); a = at<int>(w.plant(0), G::PL_LAUNCHCD); w.step(10);
        CHECK(at<int>(w.plant(0), G::PL_LAUNCHCD) - a == 5, "sunflower x0.5 slower");
    }
    { // --- 19. солнце с убитых зомби
        World w; Cfg c; c.zSunOn = true; c.zSunChance = 100; c.zSunAmount = 30; setCfg(c);
        w.addZombie(0, 0, 500.f, 200); w.addZombie(1, 20, 400.f, 100); w.addZombie(2, 0, -100.f, 100); w.step(2);
        at<uint8_t>(w.zomb(0), G::ZB_DEAD) = 1; w.step(1);
        CHECK(w.sun() == 530 && S.zSunTotal == 30, "zombie death gives sun");
        at<uint8_t>(w.zomb(1), G::ZB_DEAD) = 1; at<uint8_t>(w.zomb(2), G::ZB_DEAD) = 1; w.step(1);
        CHECK(w.sun() == 530, "bungee leaving and off-screen zombie give nothing");
        c.zSunChance = 0; setCfg(c); w.addZombie(3, 0, 300.f, 100); w.step(2); at<uint8_t>(w.zomb(3), G::ZB_DEAD) = 1; w.step(1);
        CHECK(w.sun() == 530, "chance 0 gives nothing");
        World wi; c.zSunChance = 100; setCfg(c); at<int>(wi.app, G::APP_MODE) = 61; wi.addZombie(0, 0, 500.f, 100); wi.step(2);
        at<uint8_t>(wi.zomb(0), G::ZB_DEAD) = 1; wi.step(1);
        CHECK(wi.sun() == 500, "no zombie sun in I, Zombie");
    }
    { // --- 20. достижения и выдача растений
        World w; at<int>(w.app, G::APP_UI) = 1; Cfg c; setCfg(c); bool* a = (bool*)(w.ud + G::UD_ACH);
        S.achMask = 0x5; InterlockedExchange(&S.cmd[CMD_ACH_GIVE], 1); testTick();
        CHECK(a[0] && !a[1] && a[2] && !a[3], "achievements: selected given");
        S.achMask = (1 << 21) - 1; InterlockedExchange(&S.cmd[CMD_ACH_GIVE], 1); testTick();
        bool all = true; for (int i = 0; i < 21; i++) all &= a[i];
        CHECK(all && !*(w.ud + G::UD_ACH + 21), "achievements: all 21, nothing written past them");
        S.achMask = 0x2; InterlockedExchange(&S.cmd[CMD_ACH_REVOKE], 1); testTick();
        CHECK(!a[1] && a[0] && a[2], "achievements: revoke one");
        S.plantLo = (1 << 5) | 0; S.plantHi = (1 << (40 - 32)) | (1 << (42 - 32)) | (1 << (48 - 32));
        InterlockedExchange(&S.cmd[CMD_PLANTS_PERMA], 1); testTick();
        int* pu = (int*)(w.ud + G::UD_PURCHASES);
        CHECK(pu[0] == 1 && pu[2] == 1 && pu[8] == 1 && pu[1] == 0 && pu[5] == 0, "premium plants purchased by mask");
        CHECK(at<int>(w.ud, G::UD_FINISHED) == 0, "premium issue does not touch adventure flag");
        InterlockedExchange(&S.cmd[CMD_PLANTS_ALLBASE], 1); testTick();
        CHECK(at<int>(w.ud, G::UD_FINISHED) == 1, "all base plants flag");
        World wb; Cfg c2; setCfg(c2); wb.step(2); S.plantLo = (1 << 3) | (1 << 10); S.plantHi = 0;
        InterlockedExchange(&S.cmd[CMD_PLANTS_SLOTS], 1); wb.step(1);
        CHECK(at<int>(wb.pk(0), G::SP_TYPE) == 3 && at<int>(wb.pk(1), G::SP_TYPE) == 10 && at<int>(wb.pk(2), G::SP_TYPE) == 2, "plants put into slots");
        S.plantLo = -1; S.plantHi = -1; InterlockedExchange(&S.cmd[CMD_PLANTS_SLOTS], 1); wb.step(1);
        CHECK(at<int>(wb.pk(5), G::SP_TYPE) == 5, "slots: no overflow past slot count");
    }
    { // --- 21. сборки: автозапуск по имени профиля, привязка к игроку, процент, заморозка, завершение
        runsFromIni(S.runs, ""); S.frozenBuild = -1;
        const int SBX = buildById("sandbox");
        CHECK(SBX >= kTierCount && !strcmp(kBuilds[SBX].profile, "TBG Sandbox"), "sandbox is a special build with its own profile");
        World w; at<int>(w.app, G::APP_UI) = 1; Cfg c; setCfg(c);
        at<int>(w.ud, G::UD_ID) = 77; at<int>(w.ud, G::UD_LEVEL) = 1;
        setPlayerName(w.ud, "Vasya"); w.step(1);
        CHECK(!S.runs[SBX].active && S.frozenBuild == -1, "ordinary profile name: nothing starts");
        CHECK(!strcmp(S.curPlayer, "Vasya"), "current player name is published");
        at<int>(w.ud, G::UD_ID) = 78; at<int>(w.ud, G::UD_LEVEL) = 5; setPlayerName(w.ud, "TBG Sandbox"); w.step(1);
        CHECK(!S.runs[SBX].active && S.frozenBuild == -1, "build profile with progress: run refused");
        at<int>(w.ud, G::UD_ID) = 77; at<int>(w.ud, G::UD_LEVEL) = 1; setPlayerName(w.ud, "  tbg sandbox "); w.step(1);
        CHECK(S.runs[SBX].active && S.runs[SBX].playerId == 77 && S.frozenBuild == SBX, "fresh profile with the build name: run starts by itself (case/space-insensitive)");
        CHECK(S.runs[SBX].player[0] != 0, "run remembers the player's name");
        // заморозка: настройки пользователя игнорируются, действует сборка «Песочница» (бесплатные растения)
        at<int>(w.app, G::APP_UI) = 3; c.freePlants = false; c.zCountOn = true; c.zCount = 3.f; setCfg(c);
        w.step(3);
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 1, "frozen: build settings apply (free plants)");
        CHECK(waveLen(w, 0) == 5, "frozen: user's settings are ignored (no zombie multiplier)");
        InterlockedExchange(&S.cmd[CMD_UNLOCK], 1); c.ulStore = true; setCfg(c); w.step(1);
        CHECK(at<int>(w.ud, G::UD_UPSELL) == 0 && S.cmd[CMD_UNLOCK] == 0, "frozen: commands are blocked");
        S.achMask = 1; InterlockedExchange(&S.cmd[CMD_ACH_GIVE], 1); w.step(1);
        CHECK(!*(bool*)(w.ud + G::UD_ACH), "frozen: achievements blocked");
        // прогресс
        at<int>(w.ud, G::UD_LEVEL) = 11; w.step(1); CHECK(S.runs[SBX].pct == 20, "progress: level 11 = 20%");
        at<int>(w.board, G::B_NUMWAVES) = 10; at<int>(w.board, G::B_CURWAVE) = 5; w.step(1);
        CHECK(S.runs[SBX].pct == 21, "progress: half a level adds 1%");
        at<int>(w.ud, G::UD_LEVEL) = 10; w.step(1); CHECK(S.runs[SBX].pct == 21, "progress never goes back");
        // другой игрок: заморозки нет, профиль-сборка с тем же именем у другого игрока не запускает вторую копию
        at<int>(w.ud, G::UD_ID) = 79; setPlayerName(w.ud, "Petya"); w.step(1);
        CHECK(S.frozenBuild == -1, "other player: not frozen");
        at<int>(w.ud, G::UD_ID) = 80; at<int>(w.ud, G::UD_LEVEL) = 1; setPlayerName(w.ud, "TBG Sandbox"); w.step(1);
        CHECK(S.frozenBuild == -1 && S.runs[SBX].playerId == 77, "build already running on another profile: second start refused");
        at<int>(w.ud, G::UD_ID) = 77; at<int>(w.ud, G::UD_LEVEL) = 10; w.step(1);
        CHECK(S.frozenBuild == SBX, "returning to the build's player: frozen again");
        // сериализация
        std::string ini = runsToIni(S.runs); RunState back[kMaxRuns]; runsFromIni(back, ini);
        CHECK(ini.find("build_sandbox=") != std::string::npos, "runs.ini is keyed by build id");
        CHECK(back[SBX].active && back[SBX].playerId == 77 && back[SBX].pct == 21 && !back[SBX].done, "runs.ini round trip");
        // досрочный сброс: игрок остаётся выбранным — само заново не стартует, пока не выбран другой
        S.runStopArg = SBX; InterlockedExchange(&S.cmd[CMD_RUN_STOP], 1); at<int>(w.ud, G::UD_LEVEL) = 1; w.step(2);
        CHECK(!S.runs[SBX].active && S.frozenBuild == -1, "early reset: unfrozen");
        w.step(3); CHECK(!S.runs[SBX].active && S.frozenBuild == -1, "early reset: no auto-restart on the same player");
        at<int>(w.ud, G::UD_ID) = 79; setPlayerName(w.ud, "Petya"); w.step(1);
        at<int>(w.ud, G::UD_ID) = 77; setPlayerName(w.ud, "TBG Sandbox"); w.step(1);
        CHECK(S.runs[SBX].active && S.frozenBuild == SBX, "after switching to another player and back a fresh run can start");
        // завершение
        at<int>(w.ud, G::UD_FINISHED) = 1; w.step(1);
        CHECK(S.runs[SBX].done && S.runs[SBX].pct == 100 && S.frozenBuild == -1, "adventure finished: build done, mod unfrozen");
        w.step(2); c.freePlants = false; setCfg(c); w.step(2);
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 0, "after finish: user's settings are back");
        w.step(2); CHECK(S.frozenBuild == -1 && S.runs[SBX].done, "finished profile does not freeze again");
        runsFromIni(S.runs, ""); S.frozenBuild = -1;
    }

    { // --- 22. ценность монет, новые сборки, время под заморозкой
        World w; Cfg c; c.coinValOn = true; c.coinVal = 3.f; setCfg(c); at<int>(w.ud, G::UD_MONEY) = 100; w.step(2);
        at<int>(w.ud, G::UD_MONEY) += 1; w.step(1);
        CHECK(at<int>(w.ud, G::UD_MONEY) == 103, "coin value x3: +1 -> +3");
        c.coinVal = 0.5f; setCfg(c); at<int>(w.ud, G::UD_MONEY) += 10; w.step(1);
        CHECK(at<int>(w.ud, G::UD_MONEY) == 108, "coin value x0.5: +10 -> +5");
        at<int>(w.ud, G::UD_MONEY) += 5000; w.step(1); CHECK(at<int>(w.ud, G::UD_MONEY) == 5108, "big jumps (rewards) not scaled");
        World wm; at<int>(wm.app, G::APP_UI) = 2; setCfg(c); wm.step(2); int m0 = at<int>(wm.ud, G::UD_MONEY); at<int>(wm.ud, G::UD_MONEY) += 10; wm.step(1);
        CHECK(at<int>(wm.ud, G::UD_MONEY) == m0 + 10, "money outside battle ignored");
        CHECK(kBuildCount == 15 && kBuildCount <= kMaxRuns, "15 builds fit in run slots");
        for (int i = 0; i < kBuildCount; i++) for (int j = i + 1; j < kBuildCount; j++)
            CHECK(strcmp(kBuilds[i].id, kBuilds[j].id) && strcasecmp(kBuilds[i].profile, kBuilds[j].profile), "build ids and profile names are unique");
        CHECK(buildByProfile("tbg hard") == 2 && buildByProfile("  TBG Nightmare  ") == 5 && buildByProfile("TBG") == -1 && buildByProfile("TBG Hardx") == -1 && buildByProfile("") == -1 && buildByProfile(nullptr) == -1, "buildByProfile: case/space-insensitive, exact");
        for (int i = 0; i < kBuildCount; i++) { Cfg b = buildCfg(i); CHECK(!b.invPlants && b.bgRun, "build is not a cheat cfg"); }
        RunState back[kMaxRuns];
        runsFromIni(back, "run3=1,0,55,40,Old\nrun2=1,0,5,5,Gone\nr0=1,1,9,100,60,Sand\nr10=1,0,6,10,3,Ok\nr11=1,0,6,10,3,Bad\n");
        int nI = buildById("night"), sI = buildById("sandbox"), aI = buildById("armor");
        CHECK(back[nI].active && back[nI].pct == 40 && back[nI].secs == 0 && !strcmp(back[nI].player, "Old"), "legacy run3 migrates to «night» (old runs.ini still readable)");
        CHECK(back[sI].done && back[sI].secs == 60 && back[aI].active && !strcmp(back[aI].player, "Ok"), "legacy r0/r10 migrate by build id");
        int act = 0; for (int i = 0; i < kMaxRuns; i++) act += back[i].active; CHECK(act == 3, "legacy entries of removed builds and bad indices are dropped");
        RunState rr[kMaxRuns]; int hI = buildById("hard"); rr[hI].active = 1; rr[hI].playerId = 9; rr[hI].pct = 12; rr[hI].secs = 3725; strcpy(rr[hI].player, "Ann");
        runsFromIni(back, runsToIni(rr)); CHECK(back[hI].secs == 3725 && back[hI].pct == 12 && !strcmp(back[hI].player, "Ann"), "runs.ini with time round trip");
        runsFromIni(back, "build_removed=1,0,1,1,1,X\nbuild_hard=1,0,2,3,4,Ann B\n"); CHECK(!back[0].active && back[hI].active && !strcmp(back[hI].player, "Ann B"), "unknown build ids ignored, names with spaces kept");
        // лесенка сложностей: каждая следующая строже предыдущей
        for (int i = 1; i < kTierCount; i++) {
            Cfg a = buildCfg(i - 1), b = buildCfg(i);
            auto ef = [](bool on, float v) { return on ? v : 1.f; };
            CHECK(ef(b.zCountOn, b.zCount) >= ef(a.zCountOn, a.zCount) && ef(b.zWaveOn, b.zWave) >= ef(a.zWaveOn, a.zWave) && ef(b.zSpeedOn, b.zSpeed) >= ef(a.zSpeedOn, a.zSpeed), "tiers: zombies never get easier");
            CHECK(ef(b.sunValOn, b.sunVal) <= ef(a.sunValOn, a.sunVal) && ef(b.skySunOn, b.skySun) <= ef(a.skySunOn, a.skySun) && ef(b.plantSunOn, b.plantSun) <= ef(a.plantSunOn, a.plantSun), "tiers: sun never gets more generous");
            CHECK(ef(b.atkOn, b.atkSpeed) <= ef(a.atkOn, a.atkSpeed) && ef(b.chomperSpeedOn, b.chomperSpeed) <= ef(a.chomperSpeedOn, a.chomperSpeed) && ef(b.mineSpeedOn, b.mineSpeed) <= ef(a.mineSpeedOn, a.mineSpeed), "tiers: plants never get faster");
        }
        { Cfg e = buildCfg(0); CHECK(e.sunValOn && e.sunVal == 2.f && e.skySun == 1.5f && e.atkSpeed == 1.5f && e.chomperSpeed == 2.f && e.mineSpeed == 2.f && e.zCount == 4.f && e.zWave == 2.f, "easy tier matches the spec"); }
    }

    { // --- 23. премиум без носителя: нельзя сажать без солнца, нельзя дважды списывать
        World w; Cfg c; c.premiumNoBase = true; setCfg(c); w.sun() = 100; w.step(2);
        w.addPlant(0, 40);                 // гатлинг 250 при 100 солнца
        w.step(1);
        CHECK(at<uint8_t>(w.plant(0), G::PL_DEAD) == 1, "premium: not enough sun -> planting cancelled");
        CHECK(w.sun() == 100, "premium: sun untouched when planting is cancelled");
        w.addPlant(1, 0); w.step(1);       // горохострел 100 при 100 солнца — хватает
        CHECK(at<uint8_t>(w.plant(1), G::PL_DEAD) == 0 && w.sun() == 0, "premium flag: ordinary plant charged, not free");
        w.addPlant(2, 0); w.step(1);
        CHECK(at<uint8_t>(w.plant(2), G::PL_DEAD) == 1, "premium flag: no free planting at 0 sun");
        World w2; Cfg c2; c2.premiumNoBase = true; setCfg(c2); w2.step(2);      // игра сама списала цену (флаг не отключил списание)
        w2.sun() -= 250; w2.addPlant(0, 40); w2.step(1);
        CHECK(w2.sun() == 250, "premium: cost taken by the game is not charged twice");
        World w3; setCfg(c2); at<int>(w3.app, G::APP_MODE) = 1; w3.sun() = 100; w3.step(2);
        w3.addPlant(0, 40); w3.step(1); CHECK(at<uint8_t>(w3.plant(0), G::PL_DEAD) == 1, "survival: same sun rules");
        World w4; setCfg(c2); at<int>(w4.app, G::APP_MODE) = 14; w4.sun() = 100; w4.step(2);
        w4.addPlant(0, 40); w4.step(1); CHECK(at<uint8_t>(w4.plant(0), G::PL_DEAD) == 0, "other modes: planting never cancelled");
    }
    { // --- 24. конвейер: на конвейерных уровнях премиум-логика не списывает и не отменяет посадку
        World w; Cfg c; c.premiumNoBase = true; setCfg(c); w.sun() = 0; w.step(2);
        at<int>(w.bank, G::SB_COUNT) = 5; w.step(1);                 // число семян на ленте изменилось
        w.addPlant(0, 40); w.step(1);
        CHECK(at<uint8_t>(w.plant(0), G::PL_DEAD) == 0, "conveyor level: planting not cancelled");
    }
    { // --- 25. конвейер: свой набор растений и скорость подачи
        World w; Cfg c; c.convSetOn = true; c.convLo = (1 << 3) | (1 << 5); c.convHi = 0; setCfg(c); w.step(3);
        for (int k = 0; k < 20; k++) {
            int n = at<int>(w.bank, G::SB_COUNT); if (n >= 10) n = 6;
            at<int>(w.bank, G::SB_COUNT) = n + 1; at<int>(w.pk(n), G::SP_TYPE) = 0; w.step(1);
            int t = at<int>(w.pk(n), G::SP_TYPE); CHECK(t == 3 || t == 5, "conveyor: new seed replaced with chosen plant");
            at<int>(w.bank, G::SB_COUNT) = n; w.step(1);
        }
        World w2; Cfg c2; c2.convSpeedOn = true; c2.convSpeed = 2.f; setCfg(c2);
        at<int>(w2.bank, G::SB_CONVEYOR) = 500; w2.step(1);
        for (int i = 0; i < 12; i++) { at<int>(w2.bank, G::SB_CONVEYOR) -= 1; w2.step(1); }      // игра отсчитывает вниз
        int a = at<int>(w2.bank, G::SB_CONVEYOR); at<int>(w2.bank, G::SB_CONVEYOR) = a; w2.step(1);
        int b0 = at<int>(w2.bank, G::SB_CONVEYOR); at<int>(w2.bank, G::SB_CONVEYOR) -= 1; w2.step(10);
        CHECK(at<int>(w2.bank, G::SB_CONVEYOR) < b0 - 10, "conveyor x2: counter runs faster");
        World w3; setCfg(c2); at<int>(w3.bank, G::SB_CONVEYOR) = 100; w3.step(1);
        for (int i = 0; i < 12; i++) { at<int>(w3.bank, G::SB_CONVEYOR) += 1; w3.step(1); }      // счётчик растёт — не трогаем
        int u = at<int>(w3.bank, G::SB_CONVEYOR); w3.step(10);
        CHECK(at<int>(w3.bank, G::SB_CONVEYOR) == u, "conveyor: up-counter is left alone");
    }
    { // --- 26. состав волн возвращается к оригиналу при выключении настройки
        World w; Cfg c; c.randZombies = true; c.zCountOn = true; c.zCount = 2.f; setCfg(c); w.step(3);
        CHECK(waveLen(w, 2) == 10, "waves: doubled count while enabled");
        c = Cfg(); setCfg(c); w.step(2);
        CHECK(waveLen(w, 2) == 5, "waves: original restored after switching off");
        int* l = (int*)(w.board + G::B_SPAWN_LIST) + 2 * 50; CHECK(l[0] == 0 && l[3] == 2 && l[4] == 4, "waves: original contents restored");
        c.randZombies = true; setCfg(c); w.step(2); CHECK(waveLen(w, 3) == 5, "waves: turned on mid-level rebuilds on its own");
    }

    { // --- 27. уровень без солнечной экономики (конвейер с нулём солнца): ничего не отменяется и не списывается
        World w; Cfg c; c.premiumNoBase = true; setCfg(c); w.sun() = 0; w.step(3);
        w.addPlant(0, 40); w.step(1);
        CHECK(at<uint8_t>(w.plant(0), G::PL_DEAD) == 0 && w.sun() == 0, "no-sun level: planting kept, sun untouched");
    }
    { // --- 28. случайная замена семян не срабатывает на ленте, когда семена сдвигаются
        World w; Cfg c; c.randPlants = true; setCfg(c); w.step(3);
        for (int i = 0; i < 6; i++) at<int>(w.pk(i), G::SP_TYPE) = i;
        at<int>(w.pk(2), G::SP_USED) = 1; w.step(1);                        // обычный уровень: замена работает
        CHECK(at<int>(w.pk(2), G::SP_TYPE) != 2, "random seed swap works on normal level");
        World w2; setCfg(c); w2.step(3);
        at<int>(w2.bank, G::SB_COUNT) = 5; w2.step(1);                      // лента: число семян изменилось
        for (int i = 0; i < 5; i++) at<int>(w2.pk(i), G::SP_TYPE) = i;
        at<int>(w2.pk(1), G::SP_USED) = 1; w2.step(1);
        CHECK(at<int>(w2.pk(1), G::SP_TYPE) == 1, "conveyor: seeds are not swapped randomly");
    }

    { // --- 29. разбор аудита: NaN, неверные индексы, убитые командой зомби, выданные деньги
        Cfg c; cfgFromIni(c, "zCount=nan\nsunBonusSeed=50\nconvHi=-1\natkSpeed=inf\n"); cfgSanitize(c);
        CHECK(c.zCount == 0.f && c.sunBonusSeed == 47 && c.convHi == 0xFFFF && c.atkSpeed == 10.f, "cfg: NaN/inf/out-of-range sanitised");
        Cfg d; d.atkSpeed = 2.34567881f; Cfg e; cfgFromIni(e, cfgToIni(d)); CHECK(e.atkSpeed == d.atkSpeed, "cfg: floats round-trip exactly");
        RunState rr[kMaxRuns]; runsFromIni(rr, "r99=1,0,5,10,3,Bad\nbuild_tank=1,0,6,10,3,Ok\n");
        CHECK(!rr[11].active && rr[buildById("tank")].active, "runs.ini: index outside builds ignored");
        World w; Cfg z; z.zSunOn = true; z.zSunChance = 100; z.zSunAmount = 25; setCfg(z); w.step(2);
        w.addZombie(0, 0, 400.f, 200); w.step(2);
        InterlockedExchange(&S.cmd[CMD_KILL_ZOMBIES], 1); w.step(1);
        at<uint8_t>(w.zomb(0), G::ZB_DEAD) = 1; int s0 = w.sun(); w.step(2);
        CHECK(w.sun() == s0, "killed by command: no sun bonus");
        w.addZombie(1, 0, 400.f, 200); w.step(2); at<uint8_t>(w.zomb(1), G::ZB_DEAD) = 1; w.step(1);
        CHECK(w.sun() == s0 + 25, "killed normally: sun bonus");
        World wm; Cfg m; m.coinValOn = true; m.coinVal = 3.f; m.moneyGive = 1000; setCfg(m); wm.step(2);
        InterlockedExchange(&S.cmd[CMD_GIVE_MONEY], 1); int m0 = at<int>(wm.ud, G::UD_MONEY); wm.step(2);
        CHECK(at<int>(wm.ud, G::UD_MONEY) == m0 + 100, "given money not multiplied by coin value");
    }

    { // --- 30. стартовое солнце и свой потолок
        World w; Cfg c; c.startSunOn = true; c.startSun = 200; setCfg(c); w.step(1);
        CHECK(w.sun() == 200, "start sun: set on level start"); w.step(5); CHECK(w.sun() == 200, "start sun: applied once");
        World wn; setCfg(c); wn.sun() = 0; wn.step(2); CHECK(wn.sun() == 0, "start sun: not applied on a level without sun");
        World w2; Cfg d; d.sunCapOn = true; d.sunCapVal = 300; setCfg(d); w2.sun() = 100; w2.step(2);
        w2.sun() = 900; w2.step(1); CHECK(w2.sun() == 300, "sun cap 300: a gain above the cap is trimmed to the cap");
        w2.sun() = 330; w2.step(1); CHECK(w2.sun() == 300, "sun cap 300: later gains trimmed too");
        w2.sun() = 200; w2.step(1); w2.sun() = 250; w2.step(1); CHECK(w2.sun() == 250, "sun cap 300: below the cap nothing changes");
        World wl; d.sunCapVal = 100; setCfg(d); wl.sun() = 300; wl.step(4); CHECK(wl.sun() == 300, "sun cap lowered below current sun: existing sun is kept");
        wl.sun() = 350; wl.step(1); CHECK(wl.sun() == 300, "...but it cannot grow any further");
        World wg; d.sunCapVal = 300; setCfg(d); wg.sun() = 250; wg.step(2); InterlockedExchange(&S.cmd[CMD_GIVE_SUN], 1); wg.step(1); CHECK(wg.sun() == 300, "sun cap 300: 'give sun' stops at the cap");
        World wr; Cfg rs; rs.startSunOn = true; rs.startSun = 200; setCfg(rs); at<int>(wr.board, G::B_CLOCK) = 5000; wr.step(1);
        CHECK(wr.sun() != 200, "start sun: not applied to a resumed save (clock already large)");
        World w3; d.sunCapVal = 20000; setCfg(d); w3.sun() = 5000; w3.step(2); CHECK(w3.sun() == 5000, "sun cap above 9990: no trimming");
    }
    { // --- 31. цена растений: общий множитель и по типам
        World w; Cfg c; c.costOn = true; c.costPct = 50; setCfg(c); w.step(2);
        w.sun() = 400; w.addPlant(0, 0); w.step(1);                      // игра уже списала 100 за горохострел
        CHECK(w.sun() == 450, "cost 50%: 50 refunded");
        World w2; c = Cfg(); c.costOn = true; c.costPct = 200; setCfg(c); w2.step(2);
        w2.sun() = 400; w2.addPlant(0, 0); w2.step(1);
        CHECK(w2.sun() == 300, "cost 200%: extra 100 charged");
        World w3; setCfg(c); w3.sun() = 120; w3.step(2);
        w3.sun() = 20; w3.addPlant(0, 0); w3.step(1);                       // было 120, игра списала 100, а нужно 200
        CHECK(at<uint8_t>(w3.plant(0), G::PL_DEAD) == 1 && w3.sun() == 120, "cost 200%: planting cancelled and sun returned when new price is not affordable");
        World w4; c = Cfg(); c.plantTblOn = true; c.plCost[0] = 0; setCfg(c); w4.step(2);
        w4.sun() = 400; w4.addPlant(0, 0); w4.step(1); CHECK(w4.sun() == 500, "per-type cost 0%: free peashooter");
        w4.sun() = 400; w4.addPlant(1, 1); w4.step(1); CHECK(w4.sun() == 400, "per-type cost: other plants unchanged");
        World w5; c = Cfg(); c.costOn = true; c.costPct = 50; c.freePlants = true; setCfg(c); w5.step(2);
        w5.addPlant(0, 0); w5.step(1); CHECK(w5.sun() == 500, "cost: ignored with free plants");
    }
    { // --- 32. здоровье, атака и перезарядка по типам
        World w; Cfg c; c.plantTblOn = true; c.plHp[0] = 200; c.plAtk[0] = 300; c.plSeed[2] = 300; setCfg(c); w.step(2);
        w.addPlant(0, 0); w.addPlant(1, 5); at<int>(w.plant(0), G::PL_LAUNCHCD) = 100; at<int>(w.plant(1), G::PL_LAUNCHCD) = 100; w.step(1);
        CHECK(at<int>(w.plant(0), G::PL_HP) == 600 && at<int>(w.plant(0), G::PL_HPMAX) == 600, "per-type hp x2");
        CHECK(at<int>(w.plant(1), G::PL_HP) == 300, "per-type hp: other types untouched");
        int a0 = at<int>(w.plant(0), G::PL_LAUNCHCD), a1 = at<int>(w.plant(1), G::PL_LAUNCHCD); w.step(10);
        CHECK(a0 - at<int>(w.plant(0), G::PL_LAUNCHCD) == 20, "per-type attack x3: 10 ticks remove 20 extra");
        CHECK(at<int>(w.plant(1), G::PL_LAUNCHCD) == a1, "per-type attack: other types untouched");
        for (int i : {2, 3}) { at<uint8_t>(w.pk(i), G::SP_ACTIVE) = 0; at<uint8_t>(w.pk(i), G::SP_REFRESHING) = 1; at<int>(w.pk(i), G::SP_CD) = 0; }
        w.step(10);
        CHECK(at<int>(w.pk(2), G::SP_CD) == 20 && at<int>(w.pk(3), G::SP_CD) == 0, "per-type recharge x3 only for chosen seed");
        World wg; Cfg g; g.atkOn = true; g.atkSpeed = 2.f; g.plantTblOn = true; g.plAtk[0] = 200; setCfg(g); wg.step(2);
        wg.addPlant(0, 0); at<int>(wg.plant(0), G::PL_LAUNCHCD) = 100; wg.step(1); int b0 = at<int>(wg.plant(0), G::PL_LAUNCHCD); wg.step(10);
        CHECK(b0 - at<int>(wg.plant(0), G::PL_LAUNCHCD) == 30, "global x2 and type x2 multiply (x4: 3 extra per tick)");
    }
    { // --- 33. зомби по типам
        World w; Cfg c; c.zombTblOn = true; c.zSpd[0] = 200; c.zBody[0] = 50; c.zArm[2] = 300; setCfg(c); w.step(2);
        w.addZombie(0, 0, 700.f, 200); w.addZombie(1, 2, 700.f, 200, 100); w.step(1);
        CHECK(at<int>(w.zomb(0), G::ZB_BODY) == 100 && at<int>(w.zomb(0), G::ZB_BODYMAX) == 100, "per-type zombie body x0.5");
        CHECK(at<int>(w.zomb(1), G::ZB_BODY) == 200 && at<int>(w.zomb(1), G::ZB_HELM) == 300, "per-type zombie armor x3, body untouched");
        float x0 = at<float>(w.zomb(0), G::ZB_POSX), y0 = at<float>(w.zomb(1), G::ZB_POSX);
        for (int i = 0; i < 20; i++) { at<float>(w.zomb(0), G::ZB_POSX) -= 0.25f; at<float>(w.zomb(1), G::ZB_POSX) -= 0.25f; w.step(1); }
        CHECK(fabsf((x0 - at<float>(w.zomb(0), G::ZB_POSX)) - 10.f) < 0.3f, "per-type zombie speed x2");
        CHECK(fabsf((y0 - at<float>(w.zomb(1), G::ZB_POSX)) - 5.f) < 0.3f, "per-type zombie speed: other types untouched");
        World wg; Cfg g; g.zHpOn = true; g.zHpPct = 200; g.zombTblOn = true; g.zBody[0] = 150; setCfg(g); wg.step(2);
        wg.addZombie(0, 0, 700.f, 100); wg.step(1); CHECK(at<int>(wg.zomb(0), G::ZB_BODY) == 300, "global x2 and type x1.5 multiply");
    }
    { // --- 34. ожидание первой волны
        World w; Cfg c; c.zFirstOn = true; c.zFirst = 2.f; setCfg(c); w.step(2);
        int cd0 = at<int>(w.board, G::B_ZCOUNTDOWN);
        for (int i = 0; i < 20; i++) { at<int>(w.board, G::B_ZCOUNTDOWN) -= 1; w.step(1); }
        CHECK(at<int>(w.board, G::B_ZCOUNTDOWN) >= cd0 - 12, "first wave x2: waiting twice as long (10 of 20 ticks given back)");
        World w2; c.zFirst = 0.5f; setCfg(c); w2.step(2); cd0 = at<int>(w2.board, G::B_ZCOUNTDOWN);
        for (int i = 0; i < 20; i++) { at<int>(w2.board, G::B_ZCOUNTDOWN) -= 1; w2.step(1); }
        CHECK(at<int>(w2.board, G::B_ZCOUNTDOWN) <= cd0 - 38, "first wave x0.5: countdown runs twice as fast");
        World w3; setCfg(c); at<int>(w3.board, G::B_CURWAVE) = 1; w3.step(2); cd0 = at<int>(w3.board, G::B_ZCOUNTDOWN);
        for (int i = 0; i < 20; i++) { at<int>(w3.board, G::B_ZCOUNTDOWN) -= 1; w3.step(1); }
        CHECK(at<int>(w3.board, G::B_ZCOUNTDOWN) == cd0 - 20, "first wave setting does not touch later waves");
    }
    { // --- 35. конвейер: по кругу, без повторов, пауза
        World w; Cfg c; c.convSetOn = true; c.convPick = 1; c.convLo = (1 << 3) | (1 << 5) | (1 << 7); setCfg(c); w.step(3);
        int got[9];
        for (int k = 0; k < 9; k++) {
            int n = at<int>(w.bank, G::SB_COUNT); if (n >= 10) n = 6;
            at<int>(w.bank, G::SB_COUNT) = n + 1; at<int>(w.pk(n), G::SP_TYPE) = 0; w.step(1);
            got[k] = at<int>(w.pk(n), G::SP_TYPE);
            at<int>(w.bank, G::SB_COUNT) = n; w.step(1);
        }
        bool cyc = true; static const int seq[3] = {3, 5, 7}; for (int k = 0; k < 9; k++) if (got[k] != seq[k % 3]) cyc = false;
        CHECK(cyc, "conveyor cycle: 3,5,7,3,5,7...");
        World w2; c = Cfg(); c.convSetOn = true; c.convNoRepeat = true; c.convLo = (1 << 3) | (1 << 5); setCfg(c); w2.step(3);
        int last = -1; bool rep = false;
        for (int k = 0; k < 40; k++) {
            int n = at<int>(w2.bank, G::SB_COUNT); if (n >= 10) n = 6;
            at<int>(w2.bank, G::SB_COUNT) = n + 1; at<int>(w2.pk(n), G::SP_TYPE) = 0; w2.step(1);
            int t = at<int>(w2.pk(n), G::SP_TYPE); if (t == last) rep = true; last = t;
            at<int>(w2.bank, G::SB_COUNT) = n; w2.step(1);
        }
        CHECK(!rep, "conveyor no-repeat: never the same seed twice in a row");
        World w3; Cfg p; p.convPause = true; setCfg(p); at<int>(w3.bank, G::SB_CONVEYOR) = 500; w3.step(1);
        for (int i = 0; i < 12; i++) { at<int>(w3.bank, G::SB_CONVEYOR) -= 1; w3.step(1); }
        int b0 = at<int>(w3.bank, G::SB_CONVEYOR);
        for (int i = 0; i < 20; i++) { at<int>(w3.bank, G::SB_CONVEYOR) -= 1; w3.step(1); }
        CHECK(abs(at<int>(w3.bank, G::SB_CONVEYOR) - b0) <= 2, "conveyor pause: counter stands still");
    }
    { // --- 36. настройки по типам: запись и чтение
        Cfg d; d.plantTblOn = true; d.plHp[3] = 250; d.plCost[47] = 0; d.zSpd[32] = 77; d.zArm[5] = 1500; d.convPick = 1; d.startSun = 123; d.zFirst = 2.5f;
        Cfg e; cfgFromIni(e, cfgToIni(d));
        CHECK(e.plantTblOn && e.plHp[3] == 250 && e.plCost[47] == 0 && e.zSpd[32] == 77 && e.zArm[5] == 1500 && e.plHp[4] == 100, "cfg: per-type tables round-trip");
        CHECK(e.convPick == 1 && e.startSun == 123 && e.zFirst == 2.5f, "cfg: new scalar fields round-trip");
        Cfg f; cfgFromIni(f, "plHp=5,abc,99999\nzSpd=1\nplCost=-7"); 
        CHECK(f.plHp[0] == 10 && f.plHp[1] == 10 && f.plHp[2] == 2000 && f.plHp[3] == 100, "cfg: bad table values clamped, missing ones stay 100");
        CHECK(f.zSpd[0] == 10 && f.zSpd[1] == 100 && f.plCost[0] == 0, "cfg: short or negative tables handled");
        Cfg g; cfgFromIni(g, "# старый профиль без новых полей\natkOn=1\n");
        CHECK(g.atkOn && g.plHp[10] == 100 && g.zBody[10] == 100 && !g.plantTblOn, "cfg: old profile loads with neutral tables");
    }

    { // --- 37. случайные настройки и случайное «поле»: ни падений, ни выхода за пределы
        unsigned long long rs = 88172645463325252ull;
        auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)(rs >> 11); };
        auto rv = [&]() -> std::string {
            static const char* v[] = {"0", "1", "-1", "2", "3.5", "0.1", "100", "250", "9999", "1000000", "-7", "47", "99990", "0.0001", "abc", "", "1e30", "-1e30"};
            return v[rnd() % (sizeof v / sizeof *v)];
        };
        int badSun = 0, runs = 0;
        for (int it = 0; it < 250; it++) {
            std::string ini;
#define X(n) ini += #n "=" + rv() + "\n";
            CFG_FIELDS(X)
#undef X
#define X(n, k) { ini += #n "="; for (int j = 0; j < k; j++) { if (j) ini += ","; ini += rv(); } ini += "\n"; }
            CFG_ARRAYS(X)
#undef X
            Cfg c; cfgFromIni(c, ini); setCfg(c);
            World w; w.sun() = (int)(rnd() % 3 == 0 ? rnd() % 100000 : rnd() % 1200);
            S.gameMode = (int)(rnd() % 80); at<int>(w.board, G::B_SCENE) = (int)(rnd() % 6);
            at<int>(w.board, G::B_NUMWAVES) = (int)(rnd() % 30); at<int>(w.board, G::B_ZCOUNTDOWN) = (int)(rnd() % 4000);
            for (int t = 0; t < 120; t++) {
                if (rnd() % 4 == 0) { int i = rnd() % 64; w.addPlant(i, (int)(rnd() % 70) - 5, 1 + rnd() % 5000); at<int>(w.plant(i), G::PL_IMIT) = (int)(rnd() % 52) - 2; at<int>(w.plant(i), G::PL_LAUNCHCD) = (int)(rnd() % 400); }
                if (rnd() % 4 == 0) { int i = rnd() % 64; w.addZombie(i, (int)(rnd() % 45) - 5, (float)(rnd() % 900), 1 + rnd() % 2000, rnd() % 1500, rnd() % 1500); }
                if (rnd() % 9 == 0) at<uint8_t>(w.plant(rnd() % 64), G::PL_DEAD) = 1;
                if (rnd() % 9 == 0) at<uint8_t>(w.zomb(rnd() % 64), G::ZB_DEAD) = 1;
                if (rnd() % 7 == 0) { int n = (int)(rnd() % 11); at<int>(w.bank, G::SB_COUNT) = n; for (int i = 0; i < 10; i++) { char* pk = w.pk(i); at<int>(pk, G::SP_TYPE) = (int)(rnd() % 85) - 5; at<int>(pk, G::SP_IMIT) = (int)(rnd() % 52) - 2; at<uint8_t>(pk, G::SP_REFRESHING) = rnd() % 2; at<uint8_t>(pk, G::SP_ACTIVE) = rnd() % 2; at<int>(pk, G::SP_CD) = (int)(rnd() % 600); } }
                if (rnd() % 5 == 0) at<int>(w.bank, G::SB_CONVEYOR) = (int)(rnd() % 900);
                if (rnd() % 11 == 0) at<int>(w.board, G::B_CURWAVE) = (int)(rnd() % 15);
                if (rnd() % 6 == 0) w.sun() = (int)(rnd() % 1500);
                w.step(1);
                int sn = w.sun(); if (sn < 0 || sn > 99990) badSun++;
            }
            runs++;
        }
        CHECK(badSun == 0, "fuzz: sun always stays within 0..99990 (250 random configs x 120 ticks)");
        (void)runs;
    }
    printf("\n%d проверок, %d провалов\n", checks, fails);
    return fails ? 1 : 0;
}
