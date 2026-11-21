// Настройки мода + сериализация + готовые пресеты.
#pragma once
#include <string>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

struct Cfg {
    // ---------- Растения ----------
    bool  freePlants      = false;   // все растения бесплатны (встроенный «лёгкий режим посадки»)
    int   sunGive         = 1000;    // сколько солнца выдаёт кнопка
    bool  sunBonusOn      = false;   // бонус солнц после посадки
    int   sunBonusSeed    = -1;      // -1 = любое растение, иначе тип семени
    int   sunBonusPct     = 50;      // % от стоимости растения
    bool  atkOn           = false;   // скорость атаки растений
    float atkSpeed        = 2.0f;
    bool  chomperInstant  = false;   // мгновенное «пережёвывание» чомпера
    bool  mineInstant     = false;   // мгновенный взвод картофельной мины
    bool  chomperSpeedOn  = false;   // скорость пережёвывания чомпера (×2 — вдвое быстрее, ×0.5 — вдвое дольше)
    float chomperSpeed    = 1.0f;
    bool  mineSpeedOn     = false;   // скорость взвода картофельной мины
    float mineSpeed       = 1.0f;
    bool  seedInstant     = false;   // семена без перезарядки
    bool  seedSpeedOn     = false;   // ускорение/замедление перезарядки семян
    float seedSpeed       = 2.0f;
    bool  premiumNoBase   = false;   // премиум-растения без носителя
    bool  premiumBonusOn  = false;   // бонус солнц за каждую посадку премиум-растения
    int   premiumBonusPct = 100;
    bool  hpOn            = false;   // прибавка/убавка здоровья растений
    int   hpDelta         = 0;
    bool  sunValOn        = false;   // ценность одного солнца (множитель, в обе стороны)
    float sunVal          = 1.0f;
    bool  skySunOn        = false;   // скорость появления солнца с неба
    float skySun          = 1.0f;
    bool  plantSunOn      = false;   // скорость выработки солнца подсолнухами/гриб-солнцами
    float plantSun        = 1.0f;
    bool  startSunOn      = false;   // стартовое солнце уровня (меньше или больше обычного)
    int   startSun        = 50;
    bool  sunCapOn        = false;   // свой потолок солнца (ниже или выше 9990)
    int   sunCapVal       = 9990;
    bool  costOn          = false;   // цена всех растений, % от обычной (меньше 100 — дешевле, больше — дороже)
    int   costPct         = 100;

    // ---------- Зомби ----------
    bool  zCountOn        = false;   // множитель количества зомби в волнах
    float zCount          = 1.0f;
    bool  zEqual          = false;   // «одинаковая цена»: все типы равновероятны
    bool  zWaveOn         = false;   // скорость выхода волн
    float zWave           = 1.0f;
    bool  zSpeedOn        = false;   // скорость ходьбы
    float zSpeed          = 1.0f;
    bool  zHpOn           = false;   // здоровье зомби и брони
    int   zHpPct          = 100;
    int   zArmorPct       = 100;
    bool  izCostOn        = false;   // цена зомби в режиме «Я, зомби»
    int   izCostPct       = 100;
    bool  izCostEqual     = false;
    int   izCostEqualValue= 50;
    bool  zSunOn          = false;   // с убитых зомби приходит солнце
    int   zSunChance      = 50;      // % шанс
    int   zSunAmount      = 25;      // сколько солнца за зомби
    bool  zFirstOn        = false;   // задержка до первой волны
    float zFirst          = 1.0f;    // ×: больше 1 — ждать дольше, меньше 1 — волна приходит раньше

    // ---------- Уровень ----------
    bool  evGraves        = false;
    bool  evFog           = false;
    bool  evDayNight      = false;
    int   evChance        = 50;      // % шанс каждого события после волны
    int   evGraveCount    = 3;
    bool  randPlants      = false;   // случайная замена семени после использования
    bool  randZombies     = false;   // случайный состав зомби
    int   randSetCount    = 8;       // размер случайного набора
    bool  randKeepSun     = true;    // всегда класть растение, дающее солнце

    // ---------- Конвейер ----------
    bool  convSpeedOn     = false;   // скорость подачи семян на конвейере
    float convSpeed       = 1.0f;
    bool  convSetOn       = false;   // на конвейер попадают только выбранные растения
    int   convLo          = 0;       // выбранные растения: биты 0..31
    int   convHi          = 0;       // биты 32..47
    int   convPick        = 0;       // как выбирать семя из набора: 0 — случайно, 1 — по кругу
    bool  convNoRepeat    = false;   // не класть подряд два одинаковых семени
    bool  convPause       = false;   // лента стоит: новые семена не приезжают
    unsigned long long convMask() const { return ((unsigned long long)(unsigned)convHi << 32) | (unsigned)convLo; }

    // ---------- Игра ----------
    bool  ulStore         = false;
    bool  ulAlmanac       = false;
    bool  ulModes         = false;
    bool  ulBuyAll        = false;
    int   moneyGive       = 10000;
    bool  coinValOn       = false;   // ценность монет (множитель, в бою)
    float coinVal         = 1.0f;
    int   fertGive        = 10;
    int   bugGive         = 10;
    int   chocGive        = 5;
    int   treeGive        = 5;
    int   startLevel      = 1;       // 1..50 (1-1 ... 5-10)

    // ---------- Защита, помощники, система ----------
    bool  invPlants       = false;
    bool  mushAwake       = false;
    bool  autoCollect     = false;
    bool  noSunCap        = false;
    bool  stopSpawn       = false;
    bool  bgRun           = true;
    bool  placeAnywhere   = false;
    bool  gameSpeedOn     = false;
    float gameSpeed       = 1.0f;

    // ---------- Тонкая настройка по типам (100 = как в игре) ----------
    bool  plantTblOn      = false;
    int   plHp[48], plAtk[48], plSeed[48], plCost[48];       // здоровье %, скорость атаки %, скорость перезарядки %, цена %
    bool  zombTblOn       = false;
    int   zSpd[33], zBody[33], zArm[33];                      // скорость %, здоровье тела %, броня %
    Cfg() {
        for (int i = 0; i < 48; i++) plHp[i] = plAtk[i] = plSeed[i] = plCost[i] = 100;
        for (int i = 0; i < 33; i++) zSpd[i] = zBody[i] = zArm[i] = 100;
    }
};

#define CFG_FIELDS(X) \
    X(freePlants) X(sunGive) X(sunBonusOn) X(sunBonusSeed) X(sunBonusPct) X(atkOn) X(atkSpeed) \
    X(chomperInstant) X(mineInstant) X(seedInstant) X(seedSpeedOn) X(seedSpeed) X(premiumNoBase) \
    X(premiumBonusOn) X(premiumBonusPct) X(hpOn) X(hpDelta) \
    X(sunValOn) X(sunVal) X(skySunOn) X(skySun) X(plantSunOn) X(plantSun) \
    X(zCountOn) X(zCount) X(zEqual) X(zWaveOn) X(zWave) X(zSpeedOn) X(zSpeed) X(zHpOn) X(zHpPct) \
    X(zArmorPct) X(izCostOn) X(izCostPct) X(izCostEqual) X(izCostEqualValue) \
    X(zSunOn) X(zSunChance) X(zSunAmount) \
    X(evGraves) X(evFog) X(evDayNight) X(evChance) X(evGraveCount) X(randPlants) X(randZombies) \
    X(randSetCount) X(randKeepSun) X(convSpeedOn) X(convSpeed) X(convSetOn) X(convLo) X(convHi) \
    X(chomperSpeedOn) X(chomperSpeed) X(mineSpeedOn) X(mineSpeed) X(convPick) X(convNoRepeat) X(convPause) X(startSunOn) X(startSun) X(sunCapOn) X(sunCapVal) X(costOn) X(costPct) \
    X(zFirstOn) X(zFirst) X(plantTblOn) X(zombTblOn) \
    X(ulStore) X(ulAlmanac) X(ulModes) X(ulBuyAll) X(coinValOn) X(coinVal) X(moneyGive) X(fertGive) X(bugGive) X(chocGive) \
    X(treeGive) X(startLevel) \
    X(invPlants) X(mushAwake) X(autoCollect) X(noSunCap) X(stopSpawn) X(bgRun) X(placeAnywhere) \
    X(gameSpeedOn) X(gameSpeed)

#define CFG_ARRAYS(X) X(plHp, 48) X(plAtk, 48) X(plSeed, 48) X(plCost, 48) X(zSpd, 33) X(zBody, 33) X(zArm, 33)

inline void cfgPut(std::string& o, const char* k, bool v)  { o += k; o += v ? "=1\n" : "=0\n"; }
inline void cfgPut(std::string& o, const char* k, int v)   { o += k; o += "="; o += std::to_string(v); o += "\n"; }
inline void cfgPut(std::string& o, const char* k, float v) { char b[48]; snprintf(b, sizeof b, "=%.9g\n", (double)v); o += k; o += b; }
inline void cfgGet(const std::string& s, bool& v)  { v = (s == "1" || s == "true"); }
inline void cfgGet(const std::string& s, int& v)   { v = atoi(s.c_str()); }
inline void cfgGet(const std::string& s, float& v) { v = (float)atof(s.c_str()); }

inline void cfgPutArr(std::string& o, const char* k, const int* a, int n) {
    o += k; o += "=";
    for (int i = 0; i < n; i++) { if (i) o += ","; o += std::to_string(a[i]); }
    o += "\n";
}
inline void cfgGetArr(const std::string& s, int* a, int n) {          // недостающие и битые значения остаются прежними (100)
    size_t pos = 0;
    for (int i = 0; i < n && pos <= s.size(); i++) {
        size_t e = s.find(',', pos); if (e == std::string::npos) e = s.size();
        if (e > pos) a[i] = atoi(s.substr(pos, e - pos).c_str());
        pos = e + 1;
    }
}

inline float cfgClampF(float v, float lo, float hi) { return !(v >= lo) ? lo : (v > hi ? hi : v); }      // NaN тоже уходит в lo
inline int   cfgClampI(int v, int lo, int hi)       { return v < lo ? lo : (v > hi ? hi : v); }

inline void cfgSanitize(Cfg& c) {
    c.atkSpeed   = cfgClampF(c.atkSpeed, 0.1f, 10.f);
    c.seedSpeed  = cfgClampF(c.seedSpeed, 0.1f, 20.f);
    c.zCount     = cfgClampF(c.zCount, 0.f, 6.f);
    c.zWave      = cfgClampF(c.zWave, 0.1f, 6.f);
    c.zSpeed     = cfgClampF(c.zSpeed, 0.1f, 4.f);
    c.gameSpeed  = cfgClampF(c.gameSpeed, 0.25f, 5.f);
    c.sunVal     = cfgClampF(c.sunVal, 0.1f, 10.f);
    c.skySun     = cfgClampF(c.skySun, 0.1f, 10.f);
    c.convSpeed  = cfgClampF(c.convSpeed, 0.2f, 6.f);
    c.convPick   = cfgClampI(c.convPick, 0, 1);
    c.chomperSpeed = cfgClampF(c.chomperSpeed, 0.1f, 10.f);
    c.mineSpeed  = cfgClampF(c.mineSpeed, 0.1f, 10.f);
    c.zFirst     = cfgClampF(c.zFirst, 0.1f, 10.f);
    c.startSun   = cfgClampI(c.startSun, 0, 9990);
    c.sunCapVal  = cfgClampI(c.sunCapVal, 0, 99990);
    c.costPct    = cfgClampI(c.costPct, 0, 500);
    c.convHi     = c.convHi & 0xFFFF;
    c.plantSun   = cfgClampF(c.plantSun, 0.1f, 10.f);
    c.zSunChance = cfgClampI(c.zSunChance, 0, 100);
    c.zSunAmount = cfgClampI(c.zSunAmount, 0, 1000);
    c.zHpPct     = cfgClampI(c.zHpPct, 1, 2000);
    c.zArmorPct  = cfgClampI(c.zArmorPct, 1, 2000);
    c.izCostPct  = cfgClampI(c.izCostPct, 0, 1000);
    c.izCostEqualValue = cfgClampI(c.izCostEqualValue, 0, 2000);
    c.sunBonusPct = cfgClampI(c.sunBonusPct, 0, 2000);
    c.premiumBonusPct = cfgClampI(c.premiumBonusPct, 0, 2000);
    c.hpDelta    = cfgClampI(c.hpDelta, -5000, 100000);
    c.evChance   = cfgClampI(c.evChance, 0, 100);
    c.evGraveCount = cfgClampI(c.evGraveCount, 1, 12);
    c.randSetCount = cfgClampI(c.randSetCount, 1, 10);
    c.sunGive    = cfgClampI(c.sunGive, 0, 99990);
    c.moneyGive  = cfgClampI(c.moneyGive, 0, 999990);
    c.coinVal    = cfgClampF(c.coinVal, 0.1f, 10.f);
    c.fertGive   = cfgClampI(c.fertGive, 0, 99);
    c.bugGive    = cfgClampI(c.bugGive, 0, 99);
    c.chocGive   = cfgClampI(c.chocGive, 0, 99);
    c.treeGive   = cfgClampI(c.treeGive, 0, 99);
    c.startLevel = cfgClampI(c.startLevel, 1, 50);
    c.sunBonusSeed = cfgClampI(c.sunBonusSeed, -1, 47);
    for (int i = 0; i < 48; i++) {
        c.plHp[i] = cfgClampI(c.plHp[i], 10, 2000); c.plAtk[i] = cfgClampI(c.plAtk[i], 20, 800);
        c.plSeed[i] = cfgClampI(c.plSeed[i], 20, 1000); c.plCost[i] = cfgClampI(c.plCost[i], 0, 500);
    }
    for (int i = 0; i < 33; i++) {
        c.zSpd[i] = cfgClampI(c.zSpd[i], 10, 400); c.zBody[i] = cfgClampI(c.zBody[i], 1, 2000); c.zArm[i] = cfgClampI(c.zArm[i], 1, 2000);
    }
}

inline std::string cfgToIni(const Cfg& c) {
    std::string o = "# PvZ Mod profile\n";
#define X(n) cfgPut(o, #n, c.n);
    CFG_FIELDS(X)
#undef X
#define X(n, k) cfgPutArr(o, #n, c.n, k);
    CFG_ARRAYS(X)
#undef X
    return o;
}

inline void cfgFromIni(Cfg& c, const std::string& text) {
    std::map<std::string, std::string> kv;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
#define X(n) { auto it = kv.find(#n); if (it != kv.end()) cfgGet(it->second, c.n); }
    CFG_FIELDS(X)
#undef X
#define X(n, k) { auto it = kv.find(#n); if (it != kv.end()) cfgGetArr(it->second, c.n, k); }
    CFG_ARRAYS(X)
#undef X
    cfgSanitize(c);
}

// ---------------- Что изменено: счётчики и сброс по вкладкам ----------------
// Группы совпадают с вкладками меню: 0 — Растения, 1 — Зомби, 2 — Уровень (события, случайности, конвейер, монеты, скорость игры).
// Разовые действия (выдача, разблокировка, запуск уровня) в счётчик не входят — это инструменты, а не настройки.
enum { GRP_PLANTS = 0, GRP_ZOMBIES = 1, GRP_LEVEL = 2, GRP_COUNT = 3 };

inline bool cfgTblChanged(const int* a, int n) { for (int i = 0; i < n; i++) if (a[i] != 100) return true; return false; }

// Сколько настроек группы включено (флажки) + таблица по типам считается за одну, если она включена
inline int cfgActiveCount(const Cfg& c, int g) {
    int n = 0;
    auto add = [&](bool b) { n += b ? 1 : 0; };
    if (g == GRP_PLANTS) {
        add(c.freePlants); add(c.sunBonusOn); add(c.atkOn); add(c.chomperInstant); add(c.mineInstant); add(c.chomperSpeedOn); add(c.mineSpeedOn);
        add(c.seedInstant); add(c.seedSpeedOn); add(c.premiumNoBase); add(c.premiumBonusOn); add(c.hpOn); add(c.sunValOn); add(c.skySunOn);
        add(c.plantSunOn); add(c.startSunOn); add(c.sunCapOn); add(c.costOn); add(c.invPlants); add(c.mushAwake); add(c.autoCollect);
        add(c.noSunCap); add(c.placeAnywhere); add(c.plantTblOn);
    } else if (g == GRP_ZOMBIES) {
        add(c.zCountOn); add(c.zEqual); add(c.zWaveOn); add(c.zSpeedOn); add(c.zHpOn); add(c.izCostOn); add(c.zSunOn); add(c.stopSpawn);
        add(c.zFirstOn); add(c.zombTblOn);
    } else if (g == GRP_LEVEL) {
        add(c.evGraves); add(c.evFog); add(c.evDayNight); add(c.randPlants); add(c.randZombies); add(c.convSpeedOn); add(c.convSetOn);
        add(c.convNoRepeat); add(c.convPause); add(c.coinValOn); add(c.gameSpeedOn);
    }
    return n;
}
inline int cfgActiveTotal(const Cfg& c) { int n = 0; for (int g = 0; g < GRP_COUNT; g++) n += cfgActiveCount(c, g); return n; }

// Вернуть к обычным значениям все настройки одной вкладки (не трогает инструменты, «работа в фоне» и сохранённые числа выдачи)
inline void cfgResetGroup(Cfg& c, int g) {
    Cfg d;
#define SET(f) c.f = d.f;
    if (g == GRP_PLANTS) {
        SET(freePlants) SET(sunBonusOn) SET(sunBonusSeed) SET(sunBonusPct) SET(atkOn) SET(atkSpeed) SET(chomperInstant) SET(mineInstant)
        SET(chomperSpeedOn) SET(chomperSpeed) SET(mineSpeedOn) SET(mineSpeed) SET(seedInstant) SET(seedSpeedOn) SET(seedSpeed)
        SET(premiumNoBase) SET(premiumBonusOn) SET(premiumBonusPct) SET(hpOn) SET(hpDelta) SET(sunValOn) SET(sunVal) SET(skySunOn) SET(skySun)
        SET(plantSunOn) SET(plantSun) SET(startSunOn) SET(startSun) SET(sunCapOn) SET(sunCapVal) SET(costOn) SET(costPct)
        SET(invPlants) SET(mushAwake) SET(autoCollect) SET(noSunCap) SET(placeAnywhere) SET(plantTblOn)
        memcpy(c.plHp, d.plHp, sizeof c.plHp); memcpy(c.plAtk, d.plAtk, sizeof c.plAtk); memcpy(c.plSeed, d.plSeed, sizeof c.plSeed); memcpy(c.plCost, d.plCost, sizeof c.plCost);
    } else if (g == GRP_ZOMBIES) {
        SET(zCountOn) SET(zCount) SET(zEqual) SET(zWaveOn) SET(zWave) SET(zSpeedOn) SET(zSpeed) SET(zHpOn) SET(zHpPct) SET(zArmorPct)
        SET(izCostOn) SET(izCostPct) SET(izCostEqual) SET(izCostEqualValue) SET(zSunOn) SET(zSunChance) SET(zSunAmount) SET(stopSpawn)
        SET(zFirstOn) SET(zFirst) SET(zombTblOn)
        memcpy(c.zSpd, d.zSpd, sizeof c.zSpd); memcpy(c.zBody, d.zBody, sizeof c.zBody); memcpy(c.zArm, d.zArm, sizeof c.zArm);
    } else if (g == GRP_LEVEL) {
        SET(evGraves) SET(evFog) SET(evDayNight) SET(evChance) SET(evGraveCount) SET(randPlants) SET(randZombies) SET(randSetCount) SET(randKeepSun)
        SET(convSpeedOn) SET(convSpeed) SET(convSetOn) SET(convLo) SET(convHi) SET(convPick) SET(convNoRepeat) SET(convPause)
        SET(coinValOn) SET(coinVal) SET(gameSpeedOn) SET(gameSpeed)
    }
#undef SET
}

// Отличаются ли настройки вкладки от оригинальных (включая числа в таблицах по типам и ползунки, даже если флажок снят)
inline bool cfgGroupChanged(const Cfg& c, int g) { Cfg t = c; cfgResetGroup(t, g); return cfgToIni(t) != cfgToIni(c); }

// ---------------- Готовые пресеты ----------------

// ---------------- Готовые сборки ----------------
inline void presetVanilla(Cfg& c)  { c = Cfg(); }

inline void presetSandbox(Cfg& c)  {
    c = Cfg();
    c.freePlants = true; c.premiumNoBase = true; c.seedInstant = true; c.noSunCap = true;
    c.autoCollect = true; c.mushAwake = true; c.atkOn = true; c.atkSpeed = 2.f;
    c.chomperInstant = true; c.mineInstant = true; c.hpOn = true; c.hpDelta = 300;
}
inline void presetFast(Cfg& c)     {
    c = Cfg();
    c.atkOn = true; c.atkSpeed = 2.5f; c.seedSpeedOn = true; c.seedSpeed = 3.f;
    c.zSpeedOn = true; c.zSpeed = 1.4f; c.zWaveOn = true; c.zWave = 2.f;
    c.zCountOn = true; c.zCount = 1.5f; c.autoCollect = true;
    c.skySunOn = true; c.skySun = 2.f; c.plantSunOn = true; c.plantSun = 2.f;
}
inline void presetHard(Cfg& c)     {
    c = Cfg();
    c.zHpOn = true; c.zHpPct = 150; c.zArmorPct = 250;
    c.zCountOn = true; c.zCount = 1.5f; c.zEqual = true;
    c.zSpeedOn = true; c.zSpeed = 1.2f; c.zWaveOn = true; c.zWave = 1.3f;
    c.seedSpeedOn = true; c.seedSpeed = 0.7f; c.hpOn = true; c.hpDelta = -60;
    c.sunValOn = true; c.sunVal = 0.8f;
}
inline void presetNight(Cfg& c)    {
    c = Cfg();
    c.evGraves = true; c.evFog = true; c.evDayNight = true; c.evChance = 70; c.evGraveCount = 4;
    c.zCountOn = true; c.zCount = 1.3f; c.zEqual = true; c.zWaveOn = true; c.zWave = 1.4f;
}
inline void presetChaos(Cfg& c)    {
    c = Cfg();
    c.randPlants = true; c.randZombies = true; c.zEqual = true; c.zCountOn = true; c.zCount = 1.5f;
    c.evGraves = true; c.evFog = true; c.evDayNight = true; c.evChance = 60;
    c.sunBonusOn = true; c.sunBonusSeed = -1; c.sunBonusPct = 40;
    c.zSunOn = true; c.zSunChance = 50; c.zSunAmount = 25;
}
inline void presetEasy(Cfg& c)     {
    c = Cfg();
    c.zHpOn = true; c.zHpPct = 60; c.zArmorPct = 50; c.zSpeedOn = true; c.zSpeed = 0.7f;
    c.zCountOn = true; c.zCount = 0.7f; c.sunBonusOn = true; c.sunBonusPct = 100; c.autoCollect = true;
    c.skySunOn = true; c.skySun = 1.5f;
}
inline void presetEconomy(Cfg& c)  {
    c = Cfg();
    c.sunBonusOn = true; c.sunBonusPct = 50; c.premiumNoBase = true; c.premiumBonusOn = true;
    c.premiumBonusPct = 100; c.seedSpeedOn = true; c.seedSpeed = 1.5f;
    c.plantSunOn = true; c.plantSun = 0.7f; c.zSunOn = true; c.zSunChance = 40; c.zSunAmount = 25;
}
inline void presetArmor(Cfg& c)    {
    c = Cfg();
    c.zHpOn = true; c.zHpPct = 100; c.zArmorPct = 500; c.zEqual = true; c.zCountOn = true; c.zCount = 2.f;
    c.atkOn = true; c.atkSpeed = 1.5f; c.premiumNoBase = true; c.sunBonusOn = true; c.sunBonusPct = 30;
    c.zSunOn = true; c.zSunChance = 40; c.zSunAmount = 25;
}

inline void presetScarce(Cfg& c)   {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 0.5f; c.skySunOn = true; c.skySun = 0.6f; c.plantSunOn = true; c.plantSun = 0.6f;
    c.zSunOn = true; c.zSunChance = 60; c.zSunAmount = 25; c.seedSpeedOn = true; c.seedSpeed = 0.8f;
}
inline void presetApocalypse(Cfg& c) {
    c = Cfg();
    c.zCountOn = true; c.zCount = 3.f; c.zEqual = true; c.zSpeedOn = true; c.zSpeed = 1.3f; c.zWaveOn = true; c.zWave = 1.6f;
    c.atkOn = true; c.atkSpeed = 2.f; c.seedSpeedOn = true; c.seedSpeed = 2.f; c.hpOn = true; c.hpDelta = 200;
    c.skySunOn = true; c.skySun = 2.f; c.plantSunOn = true; c.plantSun = 1.5f; c.zSunOn = true; c.zSunChance = 70; c.zSunAmount = 25;
}
inline void presetTank(Cfg& c)     {
    c = Cfg();
    c.zHpOn = true; c.zHpPct = 300; c.zArmorPct = 300; c.zSpeedOn = true; c.zSpeed = 0.6f;
    c.atkOn = true; c.atkSpeed = 2.f; c.sunBonusOn = true; c.sunBonusPct = 50;
}


// ---------------- Сложности TBG ----------------
// Лесенка от «Легко» до «Кошмара»: бонусы игрока сходят на нет и сменяются штрафами, зомби крепчают и идут гуще.
// Числа держатся в одном месте — подкручивать баланс удобно. Тест следит, чтобы каждая ступень была не легче предыдущей.
inline void presetTierEasy(Cfg& c) {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 2.f; c.skySunOn = true; c.skySun = 1.5f;
    c.atkOn = true; c.atkSpeed = 1.5f;
    c.chomperSpeedOn = true; c.chomperSpeed = 2.f; c.mineSpeedOn = true; c.mineSpeed = 2.f;
    c.startSunOn = true; c.startSun = 150; c.zFirstOn = true; c.zFirst = 2.f; c.autoCollect = true;
    c.zCountOn = true; c.zCount = 4.f; c.zWaveOn = true; c.zWave = 2.f;
}
inline void presetTierNormal(Cfg& c) {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 1.5f; c.skySunOn = true; c.skySun = 1.25f;
    c.atkOn = true; c.atkSpeed = 1.25f;
    c.chomperSpeedOn = true; c.chomperSpeed = 1.5f; c.mineSpeedOn = true; c.mineSpeed = 1.5f;
    c.startSunOn = true; c.startSun = 100; c.zFirstOn = true; c.zFirst = 1.5f; c.autoCollect = true;
    c.zCountOn = true; c.zCount = 4.f; c.zWaveOn = true; c.zWave = 2.f;
}
inline void presetTierHard(Cfg& c) {
    c = Cfg();
    c.zCountOn = true; c.zCount = 4.5f; c.zWaveOn = true; c.zWave = 2.25f;
    c.zSpeedOn = true; c.zSpeed = 1.1f; c.zHpOn = true; c.zHpPct = 125; c.zArmorPct = 150;
}
inline void presetTierInsane(Cfg& c) {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 0.8f; c.skySunOn = true; c.skySun = 0.8f; c.plantSunOn = true; c.plantSun = 0.9f;
    c.seedSpeedOn = true; c.seedSpeed = 0.85f; c.hpOn = true; c.hpDelta = -30; c.costOn = true; c.costPct = 110;
    c.zFirstOn = true; c.zFirst = 0.7f;
    c.zCountOn = true; c.zCount = 5.f; c.zWaveOn = true; c.zWave = 2.5f;
    c.zSpeedOn = true; c.zSpeed = 1.25f; c.zHpOn = true; c.zHpPct = 150; c.zArmorPct = 200; c.zEqual = true;
    c.evGraves = true; c.evFog = true; c.evDayNight = true; c.evChance = 40; c.evGraveCount = 3;
}
inline void presetTierImpossible(Cfg& c) {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 0.6f; c.skySunOn = true; c.skySun = 0.6f; c.plantSunOn = true; c.plantSun = 0.75f;
    c.atkOn = true; c.atkSpeed = 0.9f; c.chomperSpeedOn = true; c.chomperSpeed = 0.75f; c.mineSpeedOn = true; c.mineSpeed = 0.75f;
    c.seedSpeedOn = true; c.seedSpeed = 0.7f; c.hpOn = true; c.hpDelta = -60; c.costOn = true; c.costPct = 125;
    c.startSunOn = true; c.startSun = 25; c.sunCapOn = true; c.sunCapVal = 2000; c.zFirstOn = true; c.zFirst = 0.5f;
    c.zCountOn = true; c.zCount = 5.5f; c.zWaveOn = true; c.zWave = 3.f;
    c.zSpeedOn = true; c.zSpeed = 1.4f; c.zHpOn = true; c.zHpPct = 200; c.zArmorPct = 300; c.zEqual = true;
    c.evGraves = true; c.evFog = true; c.evDayNight = true; c.evChance = 60; c.evGraveCount = 4;
    c.zombTblOn = true; c.zSpd[7] = 150; c.zSpd[17] = 150;                                  // футболисты и шахтёры быстрее
}
inline void presetTierNightmare(Cfg& c) {
    c = Cfg();
    c.sunValOn = true; c.sunVal = 0.5f; c.skySunOn = true; c.skySun = 0.5f; c.plantSunOn = true; c.plantSun = 0.6f;
    c.atkOn = true; c.atkSpeed = 0.8f; c.chomperSpeedOn = true; c.chomperSpeed = 0.5f; c.mineSpeedOn = true; c.mineSpeed = 0.5f;
    c.seedSpeedOn = true; c.seedSpeed = 0.5f; c.hpOn = true; c.hpDelta = -100; c.costOn = true; c.costPct = 150;
    c.startSunOn = true; c.startSun = 25; c.sunCapOn = true; c.sunCapVal = 1000; c.zFirstOn = true; c.zFirst = 0.3f;
    c.zCountOn = true; c.zCount = 6.f; c.zWaveOn = true; c.zWave = 3.5f;
    c.zSpeedOn = true; c.zSpeed = 1.6f; c.zHpOn = true; c.zHpPct = 300; c.zArmorPct = 500; c.zEqual = true;
    c.evGraves = true; c.evFog = true; c.evDayNight = true; c.evChance = 80; c.evGraveCount = 6;
    c.zombTblOn = true; c.zBody[23] = 150; c.zBody[32] = 150; c.zArm[4] = 150;              // гаргантюа жирнее, у ведроголовых броня ещё толще
}


// Сборка = готовый набор настроек + имя игрового профиля. Прохождение начинается само, когда в игре выбран новый игрок с этим именем.
struct Build { const char* id; const char* name; const char* profile; const char* desc; void (*fn)(Cfg&); };

static const Build kBuilds[] = {
    // ----- сложности -----
    {"easy",       "Легко",             "TBG Easy",       "Солнце ×2 и с неба ×1.5, атака ×1.5, мины и чомперы ×2 — но зомби ×4, волны ×2.", presetTierEasy},
    {"normal",     "Нормально −",       "TBG Normal",     "Чуть меньше бонусов: солнце ×1.5, атака ×1.25, мины и чомперы ×1.5. Зомби ×4, волны ×2.", presetTierNormal},
    {"hard",       "Сложно",            "TBG Hard",       "Бонусов нет. Зомби ×4.5, волны ×2.25, чуть быстрее, здоровье ×1.25, броня ×1.5.", presetTierHard},
    {"insane",     "Безумно",           "TBG Insane",     "Солнце ×0.8, растения дороже и хрупче, семена медленнее. Зомби ×5 с бронёй ×2, равные типы, события 40%.", presetTierInsane},
    {"impossible", "Невозможно",        "TBG Impossible", "Солнце ×0.6, цена 125%, потолок солнца 2000, старт 25. Зомби ×5.5, быстрее ×1.4, футболисты и шахтёры ещё быстрее.", presetTierImpossible},
    {"nightmare",  "Кошмар",            "TBG Nightmare",  "Солнце ×0.5, цена 150%, потолок 1000, семена ×0.5. Зомби ×6, ×3 здоровья, броня ×5, гаргантюа жирнее, события 80%.", presetTierNightmare},
    // ----- особые сборки -----
    {"sandbox",    "Песочница",         "TBG Sandbox",    "Бесплатные растения, мгновенные семена, без лимита солнца, крепкие растения.",         presetSandbox},
    {"fast",       "Скоростной бой",    "TBG Fast",       "Всё быстрее: атака ×2.5, семена ×3, солнце ×2, зомби и волны быстрее, зомби больше.", presetFast},
    {"night",      "Адская ночь",       "TBG Night",      "Могилы, туман и смена суток после волн, плотные волны.",                               presetNight},
    {"chaos",      "Хаос",              "TBG Chaos",      "Случайные растения и зомби, все события, равные типы зомби, солнце с зомби.",          presetChaos},
    {"economy",    "Экономика выживания","TBG Economy",   "Бонус с посадок, премиум без носителя, подсолнухи медленнее, солнце с зомби.",         presetEconomy},
    {"scarce",     "Скудное солнце",    "TBG Scarce",     "Солнце вдвое дешевле, с неба и от подсолнухов приходит медленнее — выручают только зомби.", presetScarce},
    {"apocalypse", "Зомби-апокалипсис", "TBG Apocalypse", "Зомби ×3, равные типы, быстрые волны — но атака ×2, семена ×2, солнце быстрее, растения крепче.", presetApocalypse},
    {"tank",       "Танковый прорыв",   "TBG Tank",       "Тело и броня зомби ×3, но они ползут вдвое медленнее; атака ×2.",                         presetTank},
    {"armor",      "Броня-шторм",       "TBG Armor",      "Броня зомби ×5, волны вдвое плотнее, растения чуть быстрее, солнце с зомби.",          presetArmor},
};
static const int kBuildCount = (int)(sizeof(kBuilds) / sizeof(kBuilds[0]));
static const int kTierCount = 6;                      // первые kTierCount сборок — лесенка сложностей
inline Cfg buildCfg(int i) { Cfg c; if (i >= 0 && i < kBuildCount) kBuilds[i].fn(c); cfgSanitize(c); return c; }
inline int buildById(const char* id) { for (int i = 0; i < kBuildCount; i++) if (!strcmp(kBuilds[i].id, id)) return i; return -1; }
// Номер сборки по имени игрового профиля (без учёта регистра и пробелов по краям); -1 — имя не сборочное.
inline int buildByProfile(const char* name) {
    std::string n = name ? name : "";
    while (!n.empty() && n.back() == ' ') n.pop_back();
    size_t st = 0; while (st < n.size() && n[st] == ' ') st++;
    n = n.substr(st);
    for (int i = 0; i < kBuildCount; i++) {
        const char* a = kBuilds[i].profile; size_t k = 0;
        while (a[k] && k < n.size() && tolower((unsigned char)a[k]) == tolower((unsigned char)n[k])) k++;
        if (!a[k] && k == n.size()) return i;
    }
    return -1;
}
