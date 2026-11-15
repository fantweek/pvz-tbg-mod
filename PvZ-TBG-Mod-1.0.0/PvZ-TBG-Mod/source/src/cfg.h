// Настройки мода + сериализация + готовые пресеты.
#pragma once
#include <string>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    X(ulStore) X(ulAlmanac) X(ulModes) X(ulBuyAll) X(coinValOn) X(coinVal) X(moneyGive) X(fertGive) X(bugGive) X(chocGive) \
    X(treeGive) X(startLevel) \
    X(invPlants) X(mushAwake) X(autoCollect) X(noSunCap) X(stopSpawn) X(bgRun) X(placeAnywhere) \
    X(gameSpeedOn) X(gameSpeed)

inline void cfgPut(std::string& o, const char* k, bool v)  { o += k; o += v ? "=1\n" : "=0\n"; }
inline void cfgPut(std::string& o, const char* k, int v)   { o += k; o += "="; o += std::to_string(v); o += "\n"; }
inline void cfgPut(std::string& o, const char* k, float v) { char b[48]; snprintf(b, sizeof b, "=%.9g\n", (double)v); o += k; o += b; }
inline void cfgGet(const std::string& s, bool& v)  { v = (s == "1" || s == "true"); }
inline void cfgGet(const std::string& s, int& v)   { v = atoi(s.c_str()); }
inline void cfgGet(const std::string& s, float& v) { v = (float)atof(s.c_str()); }

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
}

inline std::string cfgToIni(const Cfg& c) {
    std::string o = "# PvZ Mod profile\n";
#define X(n) cfgPut(o, #n, c.n);
    CFG_FIELDS(X)
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
    cfgSanitize(c);
}

// ---------------- Готовые пресеты ----------------
struct Preset { const char* name; const char* desc; void (*fn)(Cfg&); };

// ---------------- Готовые сборки ----------------
// «Ванильный» — не сборка, а кнопка сброса всех настроек.
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

static const Preset kBuilds[] = {
    {"Песочница",            "Бесплатные растения, мгновенные семена, без лимита солнца, крепкие растения.",         presetSandbox},
    {"Скоростной бой",       "Всё быстрее: атака ×2.5, семена ×3, солнце ×2, зомби и волны быстрее, зомби больше.", presetFast},
    {"Усложнённое",          "Крепкие зомби с бронёй, равные типы, медленные семена, солнце дешевле, растения хрупче.", presetHard},
    {"Адская ночь",          "Могилы, туман и смена суток после волн, плотные волны.",                               presetNight},
    {"Хаос",                 "Случайные растения и зомби, все события, равные типы зомби, солнце с зомби.",          presetChaos},
    {"Лёгкая прогулка",      "Слабые и медленные зомби, бонус солнц за посадку, солнце с неба ×1.5.",                presetEasy},
    {"Экономика выживания",  "Бонус с посадок, премиум без носителя, подсолнухи медленнее, солнце с зомби.",         presetEconomy},
    {"Скудное солнце",       "Солнце вдвое дешевле, с неба и от подсолнухов приходит медленнее — выручают только зомби.", presetScarce},
    {"Зомби-апокалипсис",    "Зомби ×3, равные типы, быстрые волны — но атака ×2, семена ×2, солнце быстрее, растения крепче.", presetApocalypse},
    {"Танковый прорыв",      "Тело и броня зомби ×3, но они ползут вдвое медленнее; атака ×2.",                         presetTank},
    {"Броня-шторм",          "Броня зомби ×5, волны вдвое плотнее, растения чуть быстрее, солнце с зомби.",          presetArmor},
};
static const int kBuildCount = (int)(sizeof(kBuilds) / sizeof(kBuilds[0]));
inline Cfg buildCfg(int i) { Cfg c; if (i >= 0 && i < kBuildCount) kBuilds[i].fn(c); cfgSanitize(c); return c; }
