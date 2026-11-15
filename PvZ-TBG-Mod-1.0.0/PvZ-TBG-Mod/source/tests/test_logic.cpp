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
    { // --- 21. сборки: запуск, привязка к игроку, процент, заморозка, завершение
        runsFromIni(S.runs, ""); S.frozenBuild = -1;
        World w; at<int>(w.app, G::APP_UI) = 1; Cfg c; setCfg(c);
        at<int>(w.ud, G::UD_ID) = 77; at<int>(w.ud, G::UD_LEVEL) = 5;
        S.runArg = 0; InterlockedExchange(&S.cmd[CMD_RUN_START], 1); testTick();
        CHECK(!S.runs[0].active && S.frozenBuild == -1, "run refused for a player with progress");
        at<int>(w.ud, G::UD_LEVEL) = 1; InterlockedExchange(&S.cmd[CMD_RUN_START], 1); testTick();
        CHECK(S.runs[0].active && S.runs[0].playerId == 77 && S.frozenBuild == 0, "run started on a fresh player");
        S.runArg = 1; InterlockedExchange(&S.cmd[CMD_RUN_START], 1); testTick();
        CHECK(!S.runs[1].active, "same player cannot run two builds");
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
        at<int>(w.ud, G::UD_LEVEL) = 11; w.step(1); CHECK(S.runs[0].pct == 20, "progress: level 11 = 20%");
        at<int>(w.board, G::B_NUMWAVES) = 10; at<int>(w.board, G::B_CURWAVE) = 5; w.step(1);
        CHECK(S.runs[0].pct == 21, "progress: half a level adds 1%");
        at<int>(w.ud, G::UD_LEVEL) = 10; w.step(1); CHECK(S.runs[0].pct == 21, "progress never goes back");
        // другой игрок: заморозки нет
        at<int>(w.ud, G::UD_ID) = 78; w.step(1);
        CHECK(S.frozenBuild == -1, "other player: not frozen");
        at<int>(w.ud, G::UD_ID) = 77;
        // сериализация
        std::string ini = runsToIni(S.runs); RunState back[kMaxRuns]; runsFromIni(back, ini);
        CHECK(back[0].active && back[0].playerId == 77 && back[0].pct == 21 && !back[0].done, "runs.ini round trip");
        // завершение
        at<int>(w.ud, G::UD_FINISHED) = 1; w.step(1);
        CHECK(S.runs[0].done && S.runs[0].pct == 100 && S.frozenBuild == -1, "adventure finished: build done, mod unfrozen");
        w.step(2); c.freePlants = false; setCfg(c); w.step(2);
        CHECK(at<int>(w.app, G::APP_FREEPLANT) == 0, "after finish: user's settings are back");
        // сброс
        S.runStopArg = 0; InterlockedExchange(&S.cmd[CMD_RUN_STOP], 1); w.step(1);
        CHECK(!S.runs[0].active, "run reset");
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
        CHECK(kBuildCount == 11 && kBuildCount <= kMaxRuns, "11 builds fit in run slots");
        for (int i = 0; i < kBuildCount; i++) { Cfg b = buildCfg(i); CHECK(!b.invPlants && b.bgRun, "build is not a cheat cfg"); }
        RunState back[kMaxRuns]; runsFromIni(back, "run2=1,0,55,40,Old\n"); CHECK(back[2].active && back[2].pct == 40 && back[2].secs == 0 && !strcmp(back[2].player, "Old"), "old runs.ini still readable");
        RunState rr[kMaxRuns]; rr[3].active = 1; rr[3].playerId = 9; rr[3].pct = 12; rr[3].secs = 3725; strcpy(rr[3].player, "Ann");
        runsFromIni(back, runsToIni(rr)); CHECK(back[3].secs == 3725 && back[3].pct == 12 && !strcmp(back[3].player, "Ann"), "runs.ini with time round trip");
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
        RunState rr[kMaxRuns]; runsFromIni(rr, "r11=1,0,5,10,3,Bad\nr10=1,0,6,10,3,Ok\n");
        CHECK(!rr[11].active && rr[10].active, "runs.ini: index outside builds ignored");
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
    printf("\n%d проверок, %d провалов\n", checks, fails);
    return fails ? 1 : 0;
}
