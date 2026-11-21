// Общее состояние между потоком игры (мод) и потоком меню (ImGui).
#pragma once
#include "plat.h"
#include <string>
#include <cstring>

#ifndef PVZ_TEST
// Окно игры ЭТОГО процесса. FindWindow вернул бы первое окно класса «MainWindow» в системе, а такой класс бывает и у других программ.
inline BOOL CALLBACK ownWndEnum(HWND w, LPARAM lp) {
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    char cls[32] = {0}; GetClassNameA(w, cls, 31);
    if (strcmp(cls, "MainWindow") != 0) return TRUE;
    *(HWND*)lp = w; return FALSE;
}
inline HWND findOwnGameWindow() { HWND r = nullptr; EnumWindows(ownWndEnum, (LPARAM)&r); return r; }
#endif
#include "cfg.h"

enum Cmd {
    CMD_GIVE_SUN, CMD_GIVE_MONEY, CMD_GIVE_FERT, CMD_GIVE_BUG, CMD_GIVE_CHOC, CMD_GIVE_TREE,
    CMD_UNLOCK, CMD_START, CMD_START_RANDOM, CMD_RANDOM_SET, CMD_RANDOM_SET_AND_START,
    CMD_REROLL_WAVES, CMD_KILL_ZOMBIES, CMD_HP_EXISTING,
    CMD_ACH_GIVE, CMD_ACH_REVOKE,                       // достижения по маске S.achMask
    CMD_PLANTS_PERMA, CMD_PLANTS_ALLBASE, CMD_PLANTS_SLOTS,   // выдача растений (маска S.plantLo/Hi)
    CMD_RUN_STOP,                                       // досрочный сброс прохождения сборки S.runStopArg
    CMD_COUNT
};

// Прохождение готовой сборки: привязано к игровому профилю (игроку) и хранится в PvZMod\runs.ini.
static const int kMaxRuns = 24;
struct RunState {
    int  active   = 0;       // прохождение начато
    int  done     = 0;       // приключение пройдено (сборка разморожена)
    int  playerId = 0;       // PlayerInfo::mId игрока, к которому привязана сборка
    int  pct      = 0;       // процент прохождения приключения 0..100
    int  secs     = 0;       // время, проведённое в боях под заморозкой, секунд
    char player[40] = {0};   // имя игрока (для показа)
};

struct Shared {
    CRITICAL_SECTION lock;
    Cfg              cfg;                    // актуальные настройки (под lock)
    volatile LONG    cmd[CMD_COUNT];         // одноразовые команды из меню
    // статус (пишет поток игры, читает меню)
    volatile LONG    versionOk;              // exe = Steam 1.2.0.1096
    volatile LONG    hooked;                 // хук игрового потока установлен
    volatile LONG    ui;                     // 1 = меню, 2 = выбор семян, 3 = бой
    volatile LONG    sun, wave, numWaves, scene, gameMode;
    volatile LONG    hacksBad;               // число патчей, не прошедших проверку байтов
    // выдача: маски выбранных достижений / растений, номер сборки для команд RUN_*
    volatile LONG    achMask, plantLo, plantHi, runStopArg;
    // сборки
    RunState         runs[kMaxRuns];         // под lock
    volatile LONG    runSeq;                 // растёт при каждом изменении runs (меню сохраняет файл)
    volatile LONG    frozenBuild = -1;            // номер сборки, заморозившей мод, или -1
    volatile LONG    zSunTotal;              // сколько солнца принесли зомби за бой
    char             msg[320];               // последнее сообщение для пользователя
    char             curPlayer[40];          // имя выбранного в игре игрока (под lock), для вкладки «Сборки TBG»
};

// Сохранение прохождений в текст: «build_<id>=active,done,playerId,pct,secs,имя игрока».
// Ключ — стабильный id сборки, а не её номер в списке: порядок и состав сборок можно менять, не теряя прогресс.
inline std::string runsToIni(const RunState* r) {
    std::string o = "# PvZ TBG Mod: прохождения сборок\n";
    for (int i = 0; i < kBuildCount && i < kMaxRuns; i++) {
        if (!r[i].active) continue;
        char b[160]; snprintf(b, sizeof b, "=%d,%d,%d,%d,%d,", r[i].active, r[i].done, r[i].playerId, r[i].pct, r[i].secs);
        std::string nm = r[i].player; for (auto& ch : nm) if (ch == '\n' || ch == '\r') ch = ' ';
        o += "build_"; o += kBuilds[i].id; o += b; o += nm; o += "\n";
    }
    return o;
}
// Старые файлы (до 1.2): записи «r<номер>=…» / «run<номер>=…», номера — порядок сборок в версиях до 1.2.
static const char* const kLegacyBuildIds[11] = {"sandbox", "fast", nullptr, "night", "chaos", nullptr, "economy", "scarce", "apocalypse", "tank", "armor"};
inline void runsFromIni(RunState* r, const std::string& text) {
    for (int i = 0; i < kMaxRuns; i++) r[i] = RunState();
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos); if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos); pos = e + 1;
        while (!line.empty() && line.back() == '\r') line.pop_back();
        int idx = -1, a = 0, d = 0, id = 0, pc = 0, sc = 0, used = 0;
        if (line.compare(0, 6, "build_") == 0) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            idx = buildById(line.substr(6, eq - 6).c_str());
            if (idx < 0 || sscanf(line.c_str() + eq, "=%d,%d,%d,%d,%d,%n", &a, &d, &id, &pc, &sc, &used) < 5 || used <= 0) continue;
            used += (int)eq;
        } else {
            int old = -1;
            if (sscanf(line.c_str(), "r%d=%d,%d,%d,%d,%d,%n", &old, &a, &d, &id, &pc, &sc, &used) < 6 || used <= 0) {   // формат без времени
                sc = 0; used = 0;
                if (sscanf(line.c_str(), "run%d=%d,%d,%d,%d,%n", &old, &a, &d, &id, &pc, &used) < 5 || used <= 0) continue;
            }
            if (old < 0 || old >= 11 || !kLegacyBuildIds[old]) continue;       // сборки, которых больше нет, игнорируем
            idx = buildById(kLegacyBuildIds[old]);
        }
        if (idx < 0 || idx >= kMaxRuns || !a) continue;
        r[idx].active = 1; r[idx].done = d ? 1 : 0; r[idx].playerId = id;
        r[idx].pct = pc < 0 ? 0 : (pc > 100 ? 100 : pc); r[idx].secs = sc < 0 ? 0 : sc;
        strncpy(r[idx].player, line.c_str() + used, sizeof r[idx].player - 1);
    }
}

extern Shared S;
extern std::wstring g_dataDir;               // папка PvZMod рядом с DLL

void UiStart();                              // запуск потока меню (ui.cpp)
void ModSay(const char* utf8);               // сообщение в статус-строку меню

static const char* kPlantNames[48] = {
    "Горохострел", "Подсолнух", "Вишнёвая бомба", "Орех", "Картофельная мина", "Ледяной горох",
    "Чомпер", "Репитер", "Пуф-гриб", "Солнце-гриб", "Дым-гриб", "Гробокопатель", "Гипно-гриб",
    "Трусливый гриб", "Ледяной гриб", "Гриб-бомба", "Кувшинка", "Тыква-давилка (Сквош)",
    "Трёхгорох", "Ламинария", "Халапеньо", "Шипы", "Факел", "Высокий орех", "Морской гриб",
    "Фонарь", "Кактус", "Продувалка", "Раздвоенный горох", "Звёздный плод", "Тыква", "Магнит-гриб",
    "Капустомёт", "Цветочный горшок", "Кукурузомёт", "Кофейный боб", "Чеснок", "Зонтичный лист",
    "Календула", "Арбузомёт", "Гатлинг-горох", "Двойной подсолнух", "Глум-гриб", "Рогоз",
    "Зимний арбузомёт", "Золотой магнит", "Шиповая скала", "Кукурузная пушка"
};
