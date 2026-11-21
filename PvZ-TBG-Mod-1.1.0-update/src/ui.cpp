// Мод-меню на Dear ImGui (DX11), отдельное окно поверх игры. Открывается клавишей Tab.
#ifdef PVZ_SHOT                                  // сборка для скриншотов README на Linux (tools/screenshots): без WinAPI и DX11
#include "../tools/screenshots/stubs.h"
#else
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <algorithm>
#include "imgui.h"
#ifndef PVZ_SHOT
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#endif
#include "shared.h"
#include "version.h"

#ifndef PVZ_SHOT
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device*           g_dev = nullptr;
static ID3D11DeviceContext*    g_ctx = nullptr;
static IDXGISwapChain*         g_sc  = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
#endif
static HWND  g_menu = nullptr;
static bool  g_visible = false;
static float SC = 1.f;                       // масштаб DPI
static Cfg   ui;                             // рабочая копия настроек
static int   g_tab = 0;
static std::vector<std::string> g_profiles;
static char  g_pname[64] = "Мой профиль";

// ---------------------------------------------------------------- утилиты
static std::wstring W(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n); w.resize(n - 1); return w;
}
static std::string U8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr); s.resize(n - 1); return s;
}
static bool writeFile(const std::wstring& p, const std::string& d) {       // через временный файл: обрыв записи не оставит пустой ini
    std::wstring tmp = p + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n; bool ok = WriteFile(h, d.data(), (DWORD)d.size(), &n, nullptr) && n == d.size();
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
static std::string readFile(const std::wstring& p) {
    std::string out; HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    char b[4096]; DWORD n; while (ReadFile(h, b, sizeof b, &n, nullptr) && n) out.append(b, n);
    CloseHandle(h); return out;
}
static std::string safeName(std::string s) {
    for (auto& ch : s) if (strchr("\\/:*?\"<>|", ch)) ch = '_';
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s;
}
static void refreshProfiles() {
    g_profiles.clear();
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((g_dataDir + L"\\profiles\\*.ini").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { std::wstring n = fd.cFileName; g_profiles.push_back(U8(n.substr(0, n.size() - 4))); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(g_profiles.begin(), g_profiles.end());
}
static void pushCfg() {
    cfgSanitize(ui);
    EnterCriticalSection(&S.lock); S.cfg = ui; LeaveCriticalSection(&S.lock);
}
static void sendCmd(Cmd k) { pushCfg(); InterlockedExchange(&S.cmd[k], 1); }

// ---------------------------------------------------------------- оформление
static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowRounding = 10; s.ChildRounding = 10; s.FrameRounding = 6; s.GrabRounding = 6; s.PopupRounding = 8;
    s.ScrollbarRounding = 8; s.FramePadding = ImVec2(10, 6); s.ItemSpacing = ImVec2(10, 9);
    s.WindowPadding = ImVec2(16, 14); s.WindowBorderSize = 1; s.ChildBorderSize = 1; s.FrameBorderSize = 1;
    ImVec4* c = s.Colors;
    const ImVec4 bg(0.067f, 0.086f, 0.110f, 1), panel(0.102f, 0.133f, 0.173f, 1), panel2(0.133f, 0.173f, 0.224f, 1);
    const ImVec4 acc(0.435f, 0.812f, 0.290f, 1), accH(0.52f, 0.88f, 0.36f, 1), accD(0.31f, 0.62f, 0.20f, 1);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.93f, 0.95f, 1); c[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.58f, 0.64f, 1);
    c[ImGuiCol_WindowBg] = bg; c[ImGuiCol_ChildBg] = panel; c[ImGuiCol_PopupBg] = panel;
    c[ImGuiCol_Border] = ImVec4(0.27f, 0.34f, 0.43f, 1);
    c[ImGuiCol_FrameBg] = panel2; c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.22f, 0.29f, 1); c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.27f, 0.35f, 1);
    c[ImGuiCol_CheckMark] = acc; c[ImGuiCol_SliderGrab] = accD; c[ImGuiCol_SliderGrabActive] = acc;
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.24f, 0.31f, 1); c[ImGuiCol_ButtonHovered] = accD; c[ImGuiCol_ButtonActive] = acc;
    c[ImGuiCol_Header] = accD; c[ImGuiCol_HeaderHovered] = accD; c[ImGuiCol_HeaderActive] = acc;
    c[ImGuiCol_ScrollbarBg] = bg; c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.32f, 0.40f, 1);
    c[ImGuiCol_Separator] = ImVec4(0.20f, 0.26f, 0.33f, 1);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.435f, 0.812f, 0.290f, 0.35f);
    s.ScaleAllSizes(SC);
}
static const ImVec4 kAcc(0.435f, 0.812f, 0.290f, 1);

static void Hint(const char* t) {
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26); ImGui::TextUnformatted(t); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }
}
static void Note(const char* fmt, ...) {         // серый поясняющий текст с переносом строк (не обрезается по ширине окна)
    va_list ap; va_start(ap, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrappedV(fmt, ap);
    ImGui::PopStyleColor(); va_end(ap);
}
static bool Card(const char* id, const char* title) {
    ImGui::PushID(id);
    // BeginChild возвращает false, когда карточка вне видимой области (прокрутка); но EndChild/PopID вызывать всё равно надо,
    // поэтому тело карточки выполняется всегда (невидимые виджеты ImGui просто не рисует).
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextColored(kAcc, "%s", title);
    ImGui::Separator();
    return true;
}
static void EndCard() { ImGui::EndChild(); ImGui::PopID(); ImGui::Dummy(ImVec2(0, 4 * SC)); }
static float COL2() { return 360.f * SC; }

static bool SpeedBar(const char* id, float* v, float lo, float hi) {
    bool ch = false;
    ImGui::SetNextItemWidth(210 * SC);
    ch |= ImGui::SliderFloat(id, v, lo, hi, "×%.2f", ImGuiSliderFlags_Logarithmic);
    static const float q[] = {0.5f, 1.f, 2.f, 3.f};
    for (float f : q) {
        ImGui::SameLine(0, 4 * SC);
        char b[32]; snprintf(b, sizeof b, "×%g##q%s%g", f, id, f);
        if (ImGui::SmallButton(b)) { *v = f; ch = true; }
    }
    return ch;
}
static void PctBar(const char* id, int* v, int lo, int hi, const char* fmt = "%d%%") {
    ImGui::SetNextItemWidth(210 * SC); ImGui::SliderInt(id, v, lo, hi, fmt);
}

// ---------------------------------------------------------------- вкладки
static void cardPlantTypes();
static void cardZombieTypes();
static void tabPlants() {
    if (Card("p1", "Солнце")) {
        ImGui::Checkbox("Все растения бесплатны", &ui.freePlants);
        Hint("Включает встроенный «лёгкий режим посадки» игры: растения не стоят солнца, семена всегда готовы.");
        ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(130 * SC); ImGui::InputInt("##sg", &ui.sunGive, 100, 1000);
        ImGui::SameLine(); if (ImGui::Button("Выдать солнце")) sendCmd(CMD_GIVE_SUN);

        ImGui::Checkbox("Ценность одного солнца", &ui.sunValOn);
        Hint("Множитель к каждому собранному солнцу: ×2 — шарик на 25 даёт 50, ×0.5 — даёт 12–13. Работает в обе стороны и для солнца с неба, и от растений, и от зомби не действует (то задаётся отдельно). В режиме «Я, зомби» не используется.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.sunValOn); SpeedBar("##sv", &ui.sunVal, 0.1f, 10.f); ImGui::EndDisabled();
        ImGui::Checkbox("Скорость солнца с неба", &ui.skySunOn);
        Hint("Больше 1 — солнце падает чаще, меньше 1 — реже. Только на дневных уровнях, где солнце вообще падает.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.skySunOn); SpeedBar("##ss", &ui.skySun, 0.1f, 10.f); ImGui::EndDisabled();
        ImGui::Checkbox("Скорость солнечных растений", &ui.plantSunOn);
        Hint("Подсолнух, двойной подсолнух и солнце-гриб вырабатывают солнце быстрее (больше 1) или медленнее (меньше 1).");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.plantSunOn); SpeedBar("##sp", &ui.plantSun, 0.1f, 10.f); ImGui::EndDisabled();

        ImGui::Checkbox("Бонус солнц после посадки", &ui.sunBonusOn);
        Hint("После каждой посадки выбранного растения добавляется заданный процент его стоимости. 100% — растение окупается сразу.");
        ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(210 * SC);
        if (ui.sunBonusSeed < -1 || ui.sunBonusSeed > 47) ui.sunBonusSeed = -1;
        const char* cur = ui.sunBonusSeed < 0 ? "Любое растение" : kPlantNames[ui.sunBonusSeed];
        if (ImGui::BeginCombo("##sbseed", cur)) {
            if (ImGui::Selectable("Любое растение", ui.sunBonusSeed < 0)) ui.sunBonusSeed = -1;
            for (int i = 0; i < 48; i++) if (ImGui::Selectable(kPlantNames[i], ui.sunBonusSeed == i)) ui.sunBonusSeed = i;
            ImGui::EndCombo();
        }
        ImGui::SameLine(); PctBar("##sbpct", &ui.sunBonusPct, 0, 500);

        ImGui::Checkbox("Автосбор солнца и монет", &ui.autoCollect); ImGui::SameLine(COL2()); ImGui::Checkbox("Без лимита солнца (до 99 990)", &ui.noSunCap);
        EndCard();
    }
    if (Card("p5", "Стартовое солнце, потолок и цены")) {
        ImGui::Checkbox("Стартовое солнце уровня", &ui.startSunOn);
        Hint("Сколько солнца будет в начале каждого боя — меньше обычных 50 (трудно) или больше (легко). Ставится один раз при старте нового уровня (продолженная сохранёнка не меняется), только там, где в бою есть солнце (не на конвейерных уровнях).");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.startSunOn); ImGui::SetNextItemWidth(130 * SC); ImGui::InputInt("##ss0", &ui.startSun, 25, 250); ImGui::EndDisabled();
        ImGui::Checkbox("Свой потолок солнца", &ui.sunCapOn);
        Hint("Солнце не накапливается выше потолка (то, что уже есть сверх него, остаётся, пока вы не потратите). Меньше 9990 — трудно (копить нельзя), больше — игра сама держит максимум 9990, поэтому выше него мод снимает ограничение, как «Без лимита солнца».");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.sunCapOn); ImGui::SetNextItemWidth(130 * SC); ImGui::InputInt("##sc0", &ui.sunCapVal, 50, 500); ImGui::EndDisabled();
        ImGui::Checkbox("Цена всех растений", &ui.costOn);
        Hint("Процент от обычной цены: 50% — вдвое дешевле, 200% — вдвое дороже. Игра сама списывает обычную цену, мод возвращает или доплачивает разницу сразу после посадки. Дешевле обычного — солнца для самой посадки всё равно должно хватать на полную цену (проверку делает игра). Дороже — если солнца не хватает на новую цену, посадка отменяется. Цену отдельных растений можно задать внизу этой вкладки, в блоке «Растения по типам». На конвейерных уровнях и в «Я, зомби» не действует.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.costOn); PctBar("##cp0", &ui.costPct, 0, 300); ImGui::EndDisabled();
        EndCard();
    }
    if (Card("p2", "Скорость атаки и перезарядка")) {
        ImGui::Checkbox("Скорость атаки растений", &ui.atkOn);
        Hint("Больше 1 — быстрее, меньше 1 — медленнее. Работает на стрелках, мортирах, кактусе, гороху-гатлинге и т.д.");
        ImGui::SameLine(COL2()); SpeedBar("##atk", &ui.atkSpeed, 0.2f, 8.f);
        ImGui::Checkbox("Мгновенная перезарядка: Чомпер", &ui.chomperInstant);
        ImGui::SameLine(COL2()); ImGui::Checkbox("Картофельная мина (взвод)", &ui.mineInstant);
        ImGui::Checkbox("Скорость пережёвывания чомпера", &ui.chomperSpeedOn);
        Hint("Во сколько раз быстрее (больше 1) или медленнее (меньше 1) чомпер переваривает зомби. Если включена «Мгновенная перезарядка: Чомпер», она главнее.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.chomperSpeedOn || ui.chomperInstant); SpeedBar("##chs", &ui.chomperSpeed, 0.2f, 10.f); ImGui::EndDisabled();
        ImGui::Checkbox("Скорость взвода мины", &ui.mineSpeedOn);
        Hint("Во сколько раз быстрее (больше 1) или медленнее (меньше 1) картофельная мина становится готовой. Если включена «Картофельная мина (взвод)» — мгновенно.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.mineSpeedOn || ui.mineInstant); SpeedBar("##mns", &ui.mineSpeed, 0.2f, 10.f); ImGui::EndDisabled();

        ImGui::Checkbox("Перезарядка семян: без задержек", &ui.seedInstant);
        ImGui::SameLine(COL2()); ImGui::Checkbox("Свой множитель", &ui.seedSpeedOn);
        ImGui::SetCursorPosX(COL2());                                // ползунок на своей строке: рядом с флажком кнопки ×0.5…×3 не помещались в ширину окна
        ImGui::BeginDisabled(!ui.seedSpeedOn || ui.seedInstant); SpeedBar("##seed", &ui.seedSpeed, 0.2f, 10.f); ImGui::EndDisabled();
        EndCard();
    }
    if (Card("p3", "Посадка и премиум-растения")) {
        ImGui::Checkbox("Ставить премиум без носителя", &ui.premiumNoBase);
        Hint("Гатлинг, двойной подсолнух, глум, рогоз, зимний арбузомёт, золотой магнит, шиповая скала и кукурузная пушка ставятся на пустую клетку. Солнце списывается как обычно, а если его не хватает — посадка отменяется (если не включены бесплатные растения). Побочный эффект режима игры: семена всегда готовы. На конвейерных уровнях солнце не списывается.");
        ImGui::SameLine(COL2()); ImGui::Checkbox("Бонус солнц за посадку", &ui.premiumBonusOn);
        ImGui::SameLine(); ImGui::BeginDisabled(!ui.premiumBonusOn); PctBar("##pbp", &ui.premiumBonusPct, 0, 500); ImGui::EndDisabled();
        ImGui::Checkbox("Растения где угодно", &ui.placeAnywhere);
        Hint("Растения можно ставить на любую клетку, в том числе занятую.");
        EndCard();
    }
    if (Card("p4", "Здоровье и защита")) {
        ImGui::Checkbox("Изменять здоровье новых растений на", &ui.hpOn);
        ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(130 * SC); ImGui::InputInt("##hp", &ui.hpDelta, 10, 100);
        ImGui::SameLine(); if (ImGui::SmallButton("−100")) ui.hpDelta = -100;
        ImGui::SameLine(); if (ImGui::SmallButton("+100")) ui.hpDelta = 100;
        ImGui::SameLine(); if (ImGui::SmallButton("+500")) ui.hpDelta = 500;
        ImGui::SetCursorPosX(COL2()); if (ImGui::Button("Применить к уже посаженным")) sendCmd(CMD_HP_EXISTING);
        Note("Число прибавляется к здоровью (в минус — отнимается, минимум 1). Обычное здоровье растения — 300 единиц.");
        ImGui::Checkbox("Неуязвимые растения", &ui.invPlants); ImGui::SameLine(COL2()); ImGui::Checkbox("Грибы не засыпают днём", &ui.mushAwake);
        EndCard();
    }
    cardPlantTypes();
}

static void tabZombies() {
    if (Card("z1", "Состав и количество")) {
        ImGui::Checkbox("Количество зомби в волнах", &ui.zCountOn);
        Hint("Множитель числа зомби в каждой волне. ×2 — вдвое больше, ×0.5 — вдвое меньше.");
        ImGui::SameLine(COL2()); SpeedBar("##zc", &ui.zCount, 0.1f, 6.f);
        ImGui::Checkbox("Одинаковая «цена» зомби", &ui.zEqual);
        Hint("Игра набирает волны на очки: ведроголовый «стоит» дороже обычного, поэтому встречается реже. С этой галочкой все типы уровня выпадают равновероятно: ведро, конус, футболист и обычный — одинаково часто.");
        ImGui::SameLine(COL2()); if (ImGui::Button("Пересобрать оставшиеся волны")) sendCmd(CMD_REROLL_WAVES);
        ImGui::Checkbox("Остановить появление зомби", &ui.stopSpawn);
        Hint("Новые зомби перестают выходить — удобно, чтобы спокойно расставить растения.");
        Note("Состав волн меняется при старте боя, а при смене настроек посреди уровня пересобирается сам (кнопка справа — перебросить жребий заново).");
        Note("Сейчас состав волн: %s", ui.randZombies ? "случайные зомби" : ui.zEqual ? "равновероятные типы" : ui.zCountOn ? "оригинал, другое количество" : "оригинальный");
        EndCard();
    }
    if (Card("z2", "Скорость")) {
        ImGui::Checkbox("Скорость выхода волн", &ui.zWaveOn);
        Hint("Таймер до следующей волны идёт быстрее (×2 — волны вдвое чаще) или медленнее.");
        ImGui::SameLine(COL2()); SpeedBar("##zw", &ui.zWave, 0.2f, 6.f);
        ImGui::Checkbox("Скорость ходьбы зомби", &ui.zSpeedOn);
        Hint("Множитель скорости движения. Анимация ног остаётся прежней, поэтому при больших значениях зомби слегка «скользят».");
        ImGui::SameLine(COL2()); SpeedBar("##zs", &ui.zSpeed, 0.2f, 4.f);
        ImGui::Checkbox("Ожидание первой волны", &ui.zFirstOn);
        Hint("×2 — до первой волны ждать вдвое дольше (успеть расставить растения), ×0.5 — волна придёт вдвое раньше. Работает только пока первая волна не вышла.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.zFirstOn); SpeedBar("##zf", &ui.zFirst, 0.1f, 10.f); ImGui::EndDisabled();
        Note("Скорость и здоровье отдельных типов зомби — внизу этой вкладки, в блоке «Зомби по типам».");
        EndCard();
    }
    if (Card("z3", "Здоровье")) {
        ImGui::Checkbox("Здоровье зомби и броня", &ui.zHpOn);
        Hint("Применяется к каждому новому зомби. Броня — это конус, ведро, футбольный шлем, дверь, газета и т.д.");
        ImGui::SameLine(COL2()); ImGui::Text("Тело"); ImGui::SameLine(COL2() + 70 * SC); PctBar("##zhp", &ui.zHpPct, 10, 1000);
        ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine(COL2()); ImGui::Text("Броня"); ImGui::SameLine(COL2() + 70 * SC); PctBar("##zar", &ui.zArmorPct, 10, 1000);
        ImGui::SameLine(); if (ImGui::SmallButton("×5")) ui.zArmorPct = 500;
        ImGui::SameLine(); if (ImGui::Button("Убить всех зомби")) sendCmd(CMD_KILL_ZOMBIES);
        EndCard();
    }
    if (Card("z5", "Солнце с зомби")) {
        ImGui::Checkbox("С убитых зомби приходит солнце", &ui.zSunOn);
        Hint("Когда зомби погибает, с шансом ниже вам сразу зачисляется солнце. Это не «шарик, который нужно кликнуть»: настоящий шарик в игре создаёт функция, адреса которой для вашей сборки у меня нет, поэтому солнце приходит прямо в банк. Не работает в «Я, зомби».");
        ImGui::BeginDisabled(!ui.zSunOn);
        ImGui::Text("Шанс за каждого зомби"); ImGui::SameLine(COL2()); PctBar("##zsc", &ui.zSunChance, 0, 100);
        ImGui::Text("Сколько солнца"); ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(130 * SC); ImGui::InputInt("##zsa", &ui.zSunAmount, 5, 25);
        ImGui::SameLine(); if (ImGui::SmallButton("25")) ui.zSunAmount = 25;
        ImGui::SameLine(); if (ImGui::SmallButton("50")) ui.zSunAmount = 50;
        ImGui::SameLine(); if (ImGui::SmallButton("100")) ui.zSunAmount = 100;
        ImGui::EndDisabled();
        ImGui::TextDisabled("Принесли зомби за этот бой: %d", (int)S.zSunTotal);
        EndCard();
    }
    if (Card("z4", "Режим «Я, зомби»")) {
        ImGui::Checkbox("Стоимость зомби в солнцах", &ui.izCostOn);
        Hint("Цена списывается игрой, а мод корректирует её сразу после покупки. Процент от обычной цены или одна цена для всех.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.izCostOn || ui.izCostEqual); PctBar("##izp", &ui.izCostPct, 0, 300); ImGui::EndDisabled();
        ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine(COL2());
        ImGui::BeginDisabled(!ui.izCostOn); ImGui::Checkbox("Одна цена для всех", &ui.izCostEqual);
        ImGui::SameLine(); ImGui::SetNextItemWidth(110 * SC); ImGui::InputInt("##ize", &ui.izCostEqualValue, 5, 25); ImGui::EndDisabled();
        EndCard();
    }
    cardZombieTypes();
}


// ---------------------------------------------------------------- тонкая настройка по типам
static const char* kZombieNames[33] = {
    "Обычный", "Флаговый", "С конусом", "Прыгун с шестом", "С ведром", "Газетчик", "С дверью", "Футболист", "Танцор",
    "Танцор (подтанцовка)", "Утка в круге", "Ныряльщик", "Зомбони", "Бобслей", "Дельфин", "Шут (Jack-in-the-box)",
    "Шарик", "Шахтёр", "Попрыгун (пого)", "Йети", "Банджи", "Лестничник", "Катапульта", "Гаргантюа", "Бес",
    "Доктор Зомбосс", "Зомби-горохострел", "Зомби-орех", "Зомби-халапеньо", "Зомби-гатлинг", "Зомби-тыква",
    "Зомби-высокий орех", "Гига-гаргантюа"};

static void PctCell(const char* id, int* v, int lo, int hi) {            // ячейка таблицы: % от обычного значения; изменённые подсвечены
    bool ch = *v != 100;
    if (ch) ImGui::PushStyleColor(ImGuiCol_Text, kAcc);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::DragInt(id, v, 1.f, lo, hi, "%d%%");
    if (ch) ImGui::PopStyleColor();
}
// строка «Всем»: значение применяется ко всему столбцу после ввода
static void BulkCell(const char* id, int* col, int n, int lo, int hi, int* bulk) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::DragInt(id, bulk, 1.f, lo, hi, "%d%%");
    if (ImGui::IsItemDeactivatedAfterEdit()) for (int i = 0; i < n; i++) col[i] = *bulk;
}

static void cardPlantTypes() {
    if (Card("t1", "Растения по типам (каждое растение отдельно)")) {
        ImGui::Checkbox("Включить настройку по типам", &ui.plantTblOn);
        Hint("Значения — проценты от обычных (100% = как в игре). Всё работает вместе с общими настройками: общий множитель и множитель типа перемножаются. Применяется к растениям, посаженным после изменения, и к семенам/атаке сразу.");
        ImGui::SameLine(); if (ImGui::SmallButton("Сбросить всё к 100%##rp")) {
            for (int i = 0; i < 48; i++) ui.plHp[i] = ui.plAtk[i] = ui.plSeed[i] = ui.plCost[i] = 100;
        }
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.24f, 0.31f, 1));
        bool tblOpen = ImGui::CollapsingHeader("Таблица: 48 растений", ui.plantTblOn ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        ImGui::PopStyleColor();
        if (tblOpen) {
        ImGui::BeginDisabled(!ui.plantTblOn);
        static int bulk[4] = {100, 100, 100, 100};
        if (ImGui::BeginTable("##tp", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 400 * SC))) {
            ImGui::TableSetupScrollFreeze(0, 2);
            ImGui::TableSetupColumn("Растение", ImGuiTableColumnFlags_WidthFixed, 230 * SC);
            for (const char* h : {"Здоровье", "Атака", "Перезарядка", "Цена"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextColored(kAcc, "Всем растениям");
            ImGui::TableSetColumnIndex(1); BulkCell("##b0", ui.plHp, 48, 10, 2000, &bulk[0]);
            ImGui::TableSetColumnIndex(2); BulkCell("##b1", ui.plAtk, 48, 20, 800, &bulk[1]);
            ImGui::TableSetColumnIndex(3); BulkCell("##b2", ui.plSeed, 48, 20, 1000, &bulk[2]);
            ImGui::TableSetColumnIndex(4); BulkCell("##b3", ui.plCost, 48, 0, 500, &bulk[3]);
            for (int i = 0; i < 48; i++) {
                ImGui::PushID(i); ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s%s", i >= 40 ? "★ " : "", kPlantNames[i]);
                ImGui::TableSetColumnIndex(1); PctCell("##h", &ui.plHp[i], 10, 2000);
                ImGui::TableSetColumnIndex(2); PctCell("##a", &ui.plAtk[i], 20, 800);
                ImGui::TableSetColumnIndex(3); PctCell("##s", &ui.plSeed[i], 20, 1000);
                ImGui::TableSetColumnIndex(4); PctCell("##c", &ui.plCost[i], 0, 500);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("100%% = как в игре · значения перемножаются с общими настройками · зелёным отмечены изменённые");
        }
        Hint("Здоровье — новые растения. Скорость атаки — стрелки, мортиры и т.п.: 200% = вдвое быстрее. Перезарядка — скорость возврата семени в слот: 200% = вдвое быстрее. Цена — мод возвращает или доплачивает разницу сразу после посадки (см. подсказку у «Цена всех растений» во вкладке «Растения»).");
        EndCard();
    }
}
static void cardZombieTypes() {
    if (Card("t2", "Зомби по типам (каждый тип отдельно)")) {
        ImGui::Checkbox("Включить настройку по типам", &ui.zombTblOn);
        Hint("Проценты от обычных значений (100% = как в игре), перемножаются с общими настройками вкладки «Зомби». Действует на зомби, которые вышли после изменения.");
        ImGui::SameLine(); if (ImGui::SmallButton("Сбросить всё к 100%##rz")) {
            for (int i = 0; i < 33; i++) ui.zSpd[i] = ui.zBody[i] = ui.zArm[i] = 100;
        }
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.24f, 0.31f, 1));
        bool tblOpen = ImGui::CollapsingHeader("Таблица: 33 зомби", ui.zombTblOn ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        ImGui::PopStyleColor();
        if (tblOpen) {
        ImGui::BeginDisabled(!ui.zombTblOn);
        static int bulkZ[3] = {100, 100, 100};
        if (ImGui::BeginTable("##tz", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 400 * SC))) {
            ImGui::TableSetupScrollFreeze(0, 2);
            ImGui::TableSetupColumn("Зомби", ImGuiTableColumnFlags_WidthFixed, 230 * SC);
            for (const char* h : {"Скорость", "Тело", "Броня"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextColored(kAcc, "Всем зомби");
            ImGui::TableSetColumnIndex(1); BulkCell("##z0", ui.zSpd, 33, 10, 400, &bulkZ[0]);
            ImGui::TableSetColumnIndex(2); BulkCell("##z1", ui.zBody, 33, 1, 2000, &bulkZ[1]);
            ImGui::TableSetColumnIndex(3); BulkCell("##z2", ui.zArm, 33, 1, 2000, &bulkZ[2]);
            for (int i = 0; i < 33; i++) {
                ImGui::PushID(i); ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(kZombieNames[i]);
                ImGui::TableSetColumnIndex(1); PctCell("##v", &ui.zSpd[i], 10, 400);
                ImGui::TableSetColumnIndex(2); PctCell("##b", &ui.zBody[i], 1, 2000);
                ImGui::TableSetColumnIndex(3); PctCell("##r", &ui.zArm[i], 1, 2000);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("100%% = как в игре · значения перемножаются с общими настройками · зелёным отмечены изменённые");
        }
        Hint("Скорость — множитель ходьбы (анимация ног прежняя, при больших значениях зомби слегка «скользят»). Броня — конус, ведро, дверь, шлем, газета. Действует на зомби, вышедших после изменения.");
        EndCard();
    }
}

static void tabConveyor();
static void tabLevel() {
    if (Card("l1", "Случайные события после волн")) {
        ImGui::Checkbox("Могилы", &ui.evGraves); ImGui::SameLine(); ImGui::Checkbox("Туман", &ui.evFog);
        ImGui::SameLine(); ImGui::Checkbox("Смена суток", &ui.evDayNight);
        Hint("Могилы: появляются на свободных клетках справа. Туман: на ночных уровнях с туманом ветер на время сдувает его. Смена суток: день ⇄ ночь на обычных газонах; при наступлении дня грибы засыпают, ночью просыпаются.");
        ImGui::Text("Шанс каждого события после волны"); ImGui::SameLine(COL2()); PctBar("##evc", &ui.evChance, 0, 100);
        ImGui::Text("Могил за раз"); ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(210 * SC); ImGui::SliderInt("##evg", &ui.evGraveCount, 1, 12);
        EndCard();
    }
    if (Card("l2", "Случайные растения")) {
        ImGui::Checkbox("После использования семя меняется на случайное", &ui.randPlants);
        Hint("Растение, которое уже лежит в других слотах, не выпадет. Премиум-растения выпадают только если включено «Ставить премиум без носителя».");
        EndCard();
    }
    if (Card("l3", "Случайный бой")) {
        ImGui::Checkbox("Случайные зомби", &ui.randZombies);
        Hint("Состав волн собирается из случайных зомби, подходящих к типу уровня. Если выключить галочку посреди уровня — оригинальный состав возвращается сразу.");
        ImGui::Text("Размер набора растений"); ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(210 * SC); ImGui::SliderInt("##rsc", &ui.randSetCount, 1, 10);
        ImGui::Checkbox("Обязательно класть растение, дающее солнце", &ui.randKeepSun);
        Hint("Подсолнух (или солнце-гриб ночью). Снимите галочку — набор будет полностью случайным.");
        if (ImGui::Button("Случайный набор растений и запустить бой")) sendCmd(CMD_RANDOM_SET_AND_START);
        ImGui::SameLine(); if (ImGui::Button("Только выдать набор сейчас")) sendCmd(CMD_RANDOM_SET);
        Note("Из главного меню: мод выбирает уровень из вкладки «Инструменты», вам останется нажать «В бой!» — набор подставится сам.");
        EndCard();
    }
    tabConveyor();
    if (Card("l4", "Игра: монеты, скорость, система")) {
        ImGui::Checkbox("Ценность монет", &ui.coinValOn);
        Hint("Множитель к деньгам, которые вы подбираете в бою: ×2 — серебряная монета даёт вдвое больше, ×0.5 — вдвое меньше. Работает в обе стороны.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.coinValOn); SpeedBar("##cv", &ui.coinVal, 0.1f, 10.f); ImGui::EndDisabled();
        ImGui::TextDisabled("Денежные суммы округляются до десятков (в игре монета = 10).");
        ImGui::Checkbox("Игра идёт при свёрнутом окне / в меню мода", &ui.bgRun);
        Hint("Без этого игра ставится на паузу, когда окно теряет фокус (а при открытом меню фокус у меню).");
        ImGui::Checkbox("Скорость игры", &ui.gameSpeedOn); ImGui::SameLine(COL2()); SpeedBar("##gs", &ui.gameSpeed, 0.25f, 5.f);
        Note("Горячие клавиши (когда окно игры в фокусе): F6 — выдать солнце, F7 — убить всех зомби, F8 — скорость ×2 вкл/выкл. Tab — меню.");
        ImGui::Spacing();
        ImGui::TextWrapped("Расширенный экран 16:10 без чёрных полос в эту сборку не входит: для него нужно переписывать геометрию игрового поля, а готовых адресов для Steam-версии 1.2.0.1096 я не нашёл, и проверить их без самого exe нельзя. Подробности и как это доделать — в README.");
        EndCard();
    }
}

static void convSetMask(unsigned long long m) { ui.convLo = (int)(unsigned)(m & 0xffffffffull); ui.convHi = (int)(unsigned)((m >> 32) & 0xffffull); }
static unsigned long long maskOf(std::initializer_list<int> l) { unsigned long long m = 0; for (int t : l) m |= 1ull << t; return m; }

static void tabConveyor() {
    if (Card("c1", "Конвейер")) {
        Note("Уровни, где семена приезжают по ленте (боулинг с орехами и др.). На обычных уровнях эти настройки ничего не меняют.");
        ImGui::Checkbox("Скорость подачи семян", &ui.convSpeedOn);
        Hint("Больше 1 — семена приезжают на ленту чаще, меньше 1 — реже. Мод сначала наблюдает за счётчиком подачи несколько секунд и включается, только если счётчик ведёт себя как ожидается.");
        ImGui::SameLine(COL2()); ImGui::BeginDisabled(!ui.convSpeedOn); SpeedBar("##cvs", &ui.convSpeed, 0.2f, 6.f); ImGui::EndDisabled();
        ImGui::Checkbox("Лента стоит (пауза подачи)", &ui.convPause);
        Hint("Новые семена не приезжают, пока галочка стоит. Снимите — подача продолжится с того же места.");
        ImGui::Checkbox("Только выбранные растения на ленте", &ui.convSetOn);
        Hint("Каждое новое семя на ленте заменяется случайным из отмеченного списка. Если в списке пусто — настройка не работает. Учтите: на уровнях с особой механикой (боулинг) нужны именно те семена, которые задумала игра.");
        ImGui::BeginDisabled(!ui.convSetOn);
        ImGui::Text("Выбор семени"); ImGui::SameLine(COL2()); ImGui::SetNextItemWidth(210 * SC);
        ImGui::SetNextWindowSizeConstraints(ImVec2(260 * SC, 0), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginCombo("##cvp", ui.convPick == 1 ? "По кругу" : "Случайно")) {
            if (ImGui::Selectable("Случайно", ui.convPick == 0)) ui.convPick = 0;
            if (ImGui::Selectable("По кругу (по порядку списка)", ui.convPick == 1)) ui.convPick = 1;
            ImGui::EndCombo();
        }
        ImGui::SameLine(); ImGui::BeginDisabled(ui.convPick == 1); ImGui::Checkbox("Без повторов подряд", &ui.convNoRepeat); ImGui::EndDisabled();
        unsigned long long m = ui.convMask();
        if (ImGui::SmallButton("Все")) convSetMask((1ull << 48) - 1);
        ImGui::SameLine(); if (ImGui::SmallButton("Никого")) convSetMask(0);
        ImGui::SameLine(); if (ImGui::SmallButton("Атакующие")) convSetMask(maskOf({0,5,7,8,10,13,18,24,26,28,29,32,34,39,40,42,43,44}));
        ImGui::SameLine(); if (ImGui::SmallButton("Защитные")) convSetMask(maskOf({3,21,23,30,36,37,46}));
        ImGui::SameLine(); if (ImGui::SmallButton("Взрывные")) convSetMask(maskOf({2,4,14,15,17,20}));
        ImGui::SameLine(); if (ImGui::SmallButton("Солнечные")) convSetMask(maskOf({1,9,41}));
        ImGui::BeginChild("##cvgrid", ImVec2(0, 200 * SC), ImGuiChildFlags_Borders);
        for (int i = 0; i < 48; i++) {
            bool on = (m >> i) & 1ull;
            char lab[96]; snprintf(lab, sizeof lab, "%s%s##cv%d", i >= 40 ? "★ " : "", kPlantNames[i], i);
            if (ImGui::Checkbox(lab, &on)) { m = on ? (m | (1ull << i)) : (m & ~(1ull << i)); convSetMask(m); }
            if (i % 3 != 2) ImGui::SameLine(((i % 3) + 1) * 290 * SC);
        }
        ImGui::EndChild();
        ImGui::EndDisabled();
        EndCard();
    }
}

static void levelLabel(int lvl, char* b, size_t n) { snprintf(b, n, "%d-%d", (lvl - 1) / 10 + 1, (lvl - 1) % 10 + 1); }

static unsigned long long g_plantSel = 0;      // выбранные растения (бит = тип семени, 48 = имитатор)
static unsigned             g_achSel   = 0;     // выбранные достижения
static const char* kAchNames[21] = {
    "Home Lawn Security", "Nobel Peas Prize", "Better Off Dead", "China Shop", "Spudow!", "Explodonator",
    "Morticulturalist", "Don't Pea in the Pool", "Roll Some Heads", "Grounded", "Zombologist", "Penny Pincher",
    "Sunny Days", "Popcorn Party", "Good Morning", "No Fungus Among Us", "Beyond the Grave", "Immortal",
    "Towering Wisdom", "Mustache Mode", "Disco is Undead"};
static const unsigned kAchAll = (1u << 21) - 1;
static void sendMasks() {
    S.achMask = (LONG)g_achSel; S.plantLo = (LONG)(unsigned)(g_plantSel & 0xffffffffull); S.plantHi = (LONG)(unsigned)(g_plantSel >> 32);
}
static void sendCmdSel(Cmd k) { sendMasks(); sendCmd(k); }

static void tabTools() {
    if (Card("g1", "Разблокировать")) {
        ImGui::Checkbox("Магазин", &ui.ulStore); ImGui::SameLine(); ImGui::Checkbox("Альманах", &ui.ulAlmanac);
        ImGui::SameLine(); ImGui::Checkbox("Мини-игры, головоломки, выживание", &ui.ulModes);
        ImGui::SameLine(); ImGui::Checkbox("Всё в магазине куплено", &ui.ulBuyAll);
        Hint("Магазин и альманах в игре открываются флагом «приключение пройдено» — вместе с ним открывается всё, что к этому флагу привязано. Резервная копия профиля сохраняется в папке PvZMod\\backup при первом запуске.");
        if (ImGui::Button("Применить разблокировку")) sendCmd(CMD_UNLOCK);
        EndCard();
    }
    if (Card("g2", "Запуск уровня")) {
        char lab[16]; levelLabel(ui.startLevel, lab, sizeof lab);
        ImGui::SetNextItemWidth(120 * SC);
        if (ImGui::BeginCombo("##lvl", lab)) {
            for (int i = 1; i <= 50; i++) { char l[16]; levelLabel(i, l, sizeof l); if (ImGui::Selectable(l, ui.startLevel == i)) ui.startLevel = i; }
            ImGui::EndCombo();
        }
        ImGui::SameLine(); if (ImGui::Button("Запустить выбранный")) sendCmd(CMD_START);
        ImGui::SameLine(); if (ImGui::Button("Случайный уровень")) sendCmd(CMD_START_RANDOM);
        Note("Работает из главного меню игры. Мод выставляет уровень в профиле и нажимает «Приключение»; если клик не попал — нажмите кнопку сами.");
        Note("Внимание: пройденный уровень сдвигает прогресс профиля. Копия профиля лежит в PvZMod\\backup.");
        EndCard();
    }
    if (Card("g3", "Выдача денег и предметов сада")) {
        ImGui::Text("Деньги"); ImGui::SameLine(160 * SC); ImGui::SetNextItemWidth(150 * SC); ImGui::InputInt("##mg", &ui.moneyGive, 1000, 10000);
        ImGui::SameLine(); if (ImGui::Button("Выдать##m")) sendCmd(CMD_GIVE_MONEY);
        ImGui::Text("Удобрение"); ImGui::SameLine(160 * SC); ImGui::SetNextItemWidth(150 * SC); ImGui::InputInt("##fg", &ui.fertGive);
        ImGui::SameLine(); if (ImGui::Button("Выдать##f")) sendCmd(CMD_GIVE_FERT);
        ImGui::Text("Спрей от жуков"); ImGui::SameLine(160 * SC); ImGui::SetNextItemWidth(150 * SC); ImGui::InputInt("##bg", &ui.bugGive);
        ImGui::SameLine(); if (ImGui::Button("Выдать##b")) sendCmd(CMD_GIVE_BUG);
        ImGui::Text("Шоколад"); ImGui::SameLine(160 * SC); ImGui::SetNextItemWidth(150 * SC); ImGui::InputInt("##cg", &ui.chocGive);
        ImGui::SameLine(); if (ImGui::Button("Выдать##c")) sendCmd(CMD_GIVE_CHOC);
        ImGui::Text("Еда для дерева"); ImGui::SameLine(160 * SC); ImGui::SetNextItemWidth(150 * SC); ImGui::InputInt("##tg", &ui.treeGive);
        ImGui::SameLine(); if (ImGui::Button("Выдать##t")) sendCmd(CMD_GIVE_TREE);
        EndCard();
    }
    if (Card("g4", "Выдача растений")) {
        ImGui::TextDisabled("Отметьте одно, несколько или все растения:");
        ImGui::BeginChild("##plgrid", ImVec2(0, 190 * SC), ImGuiChildFlags_Borders);
        for (int i = 0; i < 49; i++) {
            bool on = (g_plantSel >> i) & 1ull;
            char lab[96]; snprintf(lab, sizeof lab, "%s%s##pl%d", i >= 40 ? "★ " : "", i < 48 ? kPlantNames[i] : "Имитатор", i);
            if (ImGui::Checkbox(lab, &on)) g_plantSel = on ? (g_plantSel | (1ull << i)) : (g_plantSel & ~(1ull << i));
            if (i % 3 != 2 && i != 48) ImGui::SameLine(((i % 3) + 1) * 290 * SC);
        }
        ImGui::EndChild();
        if (ImGui::SmallButton("Выбрать все")) g_plantSel = (1ull << 49) - 1;
        ImGui::SameLine(); if (ImGui::SmallButton("Снять выбор")) g_plantSel = 0;
        ImGui::SameLine(); if (ImGui::SmallButton("Только премиум ★")) g_plantSel = ((1ull << 49) - 1) & ~((1ull << 40) - 1);
        if (ImGui::Button("Положить в слоты боя")) sendCmdSel(CMD_PLANTS_SLOTS);
        Hint("В бою кладёт выбранные растения в слоты семян (первые N слотов), любые из 48 — независимо от того, открыты ли они. Нужен начавшийся бой.");
        ImGui::SameLine(); if (ImGui::Button("Открыть премиум ★ навсегда")) sendCmdSel(CMD_PLANTS_PERMA);
        Hint("Премиум-растения (★) игра открывает покупкой в магазине — мод ставит флаг покупки только для отмеченных. Бесплатно и навсегда в этом профиле.");
        ImGui::SameLine(); if (ImGui::Button("Открыть все обычные")) sendCmd(CMD_PLANTS_ALLBASE);
        Hint("Обычные растения игра открывает строго по порядку прохождения приключения, поэтому поштучно их открыть нельзя. Эта кнопка открывает все сразу (флаг «приключение пройдено», как в «Разблокировать»). Поштучно любое растение можно выдать кнопкой «Положить в слоты боя».");
        EndCard();
    }
    if (Card("g5", "Достижения")) {
        Note("Достижения профиля игры (список — «Достижения» в главном меню). Steam-достижения не затрагиваются.");
        for (int i = 0; i < 21; i++) {
            bool on = (g_achSel >> i) & 1u;
            char lab[64]; snprintf(lab, sizeof lab, "%s##ac%d", kAchNames[i], i);
            if (ImGui::Checkbox(lab, &on)) g_achSel = on ? (g_achSel | (1u << i)) : (g_achSel & ~(1u << i));
            if (i % 3 != 2 && i != 20) ImGui::SameLine(((i % 3) + 1) * 290 * SC);
        }
        if (ImGui::SmallButton("Выбрать все")) g_achSel = kAchAll;
        ImGui::SameLine(); if (ImGui::SmallButton("Снять выбор")) g_achSel = 0;
        if (ImGui::Button("Выдать отмеченные")) sendCmdSel(CMD_ACH_GIVE);
        ImGui::SameLine(); if (ImGui::Button("Выдать все 21")) { g_achSel = kAchAll; sendCmdSel(CMD_ACH_GIVE); }
        ImGui::SameLine(); if (ImGui::Button("Забрать отмеченные")) sendCmdSel(CMD_ACH_REVOKE);
        Note("Выданные достижения игра покажет в окне наград и сохранит вместе с профилем при выходе в меню или из игры.");
        EndCard();
    }
}

static void buildRow(int i, int frozen, const RunState& r, const char* curPlayer) {
    ImGui::PushID(i);
    bool running = r.active && !r.done;
    bool here = curPlayer[0] && buildByProfile(curPlayer) == i;
    ImGui::BeginChild("##b", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextColored(running ? ImVec4(1.f, 0.8f, 0.3f, 1) : (r.done ? kAcc : ImVec4(0.9f, 0.93f, 0.95f, 1)), "%s", kBuilds[i].name);
    ImGui::SameLine(); ImGui::TextDisabled("  профиль: «%s»", kBuilds[i].profile);
    if (frozen == i) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1), "  ● идёт, мод заморожен"); }
    else if (r.done) { ImGui::SameLine(); ImGui::TextColored(kAcc, "  ✓ пройдена"); }
    else if (here && !running) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.f, 1), "  выбран в игре"); }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("%s", kBuilds[i].desc); ImGui::PopStyleColor();
    if (r.active) {
        char ov[128]; snprintf(ov, sizeof ov, "%d%%  ·  %d:%02d:%02d в боях  ·  игрок «%s»", r.pct, r.secs / 3600, r.secs / 60 % 60, r.secs % 60, r.player[0] ? r.player : "?");
        ImGui::ProgressBar(r.pct / 100.f, ImVec2(380 * SC, 0), ov);
        ImGui::SameLine(); if (ImGui::Button("Сбросить")) { S.runStopArg = i; sendCmd(CMD_RUN_STOP); }
    } else {
        ImGui::TextDisabled("Не начата — создайте игрока «%s» и выберите его в игре", kBuilds[i].profile);
    }
    ImGui::EndChild();
    ImGui::PopID();
}

static void tabBuilds(int frozen) {
    RunState rs[kMaxRuns]; char cur[sizeof S.curPlayer];
    EnterCriticalSection(&S.lock); for (int i = 0; i < kMaxRuns; i++) rs[i] = S.runs[i]; memcpy(cur, S.curPlayer, sizeof cur); LeaveCriticalSection(&S.lock);
    cur[sizeof cur - 1] = 0;

    ImGui::BeginDisabled(frozen >= 0);
    if (Card("b0", "Сброс до обычной игры")) {
        if (ImGui::Button("Сбросить все настройки до ванильных")) { presetVanilla(ui); pushCfg(); ModSay("Все настройки сброшены — игра как в оригинале."); }
        ImGui::SameLine(); ImGui::TextDisabled("без сборки, просто всё как в оригинале");
        EndCard();
    }
    ImGui::EndDisabled();

    if (Card("b1", "Сборки TBG")) {
        ImGui::TextWrapped("Сборка — это набор настроек на всё приключение. У каждой есть свой профиль игрока с точным именем. Создайте его в игре («Сменить игрока» → «Новый игрок») и выберите — как только мод увидит игрока с таким именем и без прогресса, сборка начнётся сама, а мод заморозится: действуют только настройки сборки.");
        Note("Заморозка снимается, когда приключение пройдено, когда вы выбираете в игре другого игрока или когда нажмёте «Сбросить» у сборки. Прогресс — по уровням приключения (50 уровней).");
        char who[96]; snprintf(who, sizeof who, "Сейчас в игре выбран игрок: %s", cur[0] ? cur : "—");
        ImGui::TextDisabled("%s", who);
        ImGui::Spacing();
        ImGui::TextColored(kAcc, "Сложности");
        for (int i = 0; i < kTierCount; i++) buildRow(i, frozen, rs[i], cur);
        ImGui::Spacing();
        ImGui::TextColored(kAcc, "Особые сборки");
        for (int i = kTierCount; i < kBuildCount; i++) buildRow(i, frozen, rs[i], cur);
        EndCard();
    }
    ImGui::BeginDisabled(frozen >= 0);
    if (Card("f1", "Мои конфигурации")) {
        ImGui::SetNextItemWidth(260 * SC); ImGui::InputText("##pn", g_pname, sizeof g_pname);
        ImGui::SameLine();
        if (ImGui::Button("Сохранить")) {
            std::string n = safeName(g_pname);
            if (!n.empty()) { pushCfg(); writeFile(g_dataDir + L"\\profiles\\" + W(n) + L".ini", cfgToIni(ui)); refreshProfiles(); ModSay("Конфигурация сохранена."); }
        }
        ImGui::SameLine(); if (ImGui::Button("Обновить список")) refreshProfiles();
        ImGui::Spacing();
        std::string del;
        for (auto& n : g_profiles) {
            ImGui::PushID(n.c_str());
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(n.c_str());
            ImGui::SameLine(340 * SC);
            if (ImGui::Button("Загрузить")) {
                std::string t = readFile(g_dataDir + L"\\profiles\\" + W(n) + L".ini");
                if (t.empty()) ModSay("Не удалось прочитать файл конфигурации.");
                else { Cfg fresh; cfgFromIni(fresh, t); ui = fresh; pushCfg(); ModSay("Конфигурация загружена."); }     // поля, которых нет в файле, берутся по умолчанию
            }
            ImGui::SameLine(); if (ImGui::Button("Удалить")) del = n;
            ImGui::PopID();
        }
        if (!del.empty()) { DeleteFileW((g_dataDir + L"\\profiles\\" + W(del) + L".ini").c_str()); refreshProfiles(); }
        if (g_profiles.empty()) ImGui::TextDisabled("Сохранённых конфигураций пока нет.");
        Note("Текущие настройки автоматически сохраняются и подхватываются при следующем запуске.");
        EndCard();
    }
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------- отрисовка кадра
static void drawUi() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // заголовок (перетаскивание окна)
    ImGui::TextColored(kAcc, "PvZ TBG MOD"); ImGui::SameLine(); ImGui::TextDisabled("v%s · Plants vs. Zombies GOTY · Tab — закрыть", TBG_VERSION);
    const int fzNow = (int)S.frozenBuild;
    Cfg shown = fzNow >= 0 ? buildCfg(fzNow) : ui;                 // что реально действует: настройки сборки при заморозке, иначе ваши
    int activeTotal = cfgActiveTotal(shown);
    ImGui::SameLine(); ImGui::TextColored(activeTotal ? kAcc : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "  ·  включено настроек: %d", activeTotal);
    ImVec2 tp = ImGui::GetWindowPos(); float titleH = ImGui::GetFrameHeight() + 8 * SC;
    if (ImGui::IsMouseClicked(0) && ImGui::GetMousePos().y < tp.y + titleH && ImGui::GetMousePos().x < tp.x + io.DisplaySize.x - 60 * SC) {
        ReleaseCapture(); SendMessageW(g_menu, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    ImGui::SameLine(io.DisplaySize.x - 50 * SC);
    if (ImGui::SmallButton("  X  ")) { g_visible = false; ShowWindow(g_menu, SW_HIDE); }
    ImGui::Separator();

    float sideW = 190 * SC;
    ImGui::BeginChild("##side", ImVec2(sideW, -ImGui::GetFrameHeightWithSpacing() * 1.6f), ImGuiChildFlags_None);
    static const char* tabs[] = {"Растения", "Зомби", "Уровень", "Инструменты", "Сборки TBG"};
    for (int i = 0; i < 5; i++) {
        bool sel = g_tab == i;
        if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31f, 0.62f, 0.20f, 1));
        if (ImGui::Button(tabs[i], ImVec2(-1, 46 * SC))) { g_tab = i; if (i == 4) refreshProfiles(); }
        if (sel) ImGui::PopStyleColor();
        if (i < GRP_COUNT) {                                         // значок «сколько настроек включено» на вкладке
            int n = cfgActiveCount(shown, i);
            if (n > 0) {
                char b[16]; snprintf(b, sizeof b, "%d", n);
                ImVec2 mx = ImGui::GetItemRectMax(), mn = ImGui::GetItemRectMin(), ts = ImGui::CalcTextSize(b);
                float r = (ts.x > ts.y ? ts.x : ts.y) * 0.5f + 5 * SC; ImVec2 ctr(mx.x - r - 8 * SC, (mn.y + mx.y) * 0.5f);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddCircleFilled(ctr, r, ImGui::GetColorU32(sel ? ImVec4(0.07f, 0.09f, 0.11f, 1) : ImVec4(0.31f, 0.62f, 0.20f, 1)));
                dl->AddText(ImVec2(ctr.x - ts.x * 0.5f, ctr.y - ts.y * 0.5f), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), b);
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##content", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.6f), ImGuiChildFlags_None);
    int fz = (int)S.frozenBuild;
    if (fz >= 0) {                                   // мод заморожен сборкой: показываем её настройки, менять нельзя
        RunState r; EnterCriticalSection(&S.lock); r = S.runs[fz]; LeaveCriticalSection(&S.lock);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.8f, 0.3f, 1));
        ImGui::TextWrapped("Мод заморожен сборкой «%s» — %d%%. Настройки ниже включены сборкой и не меняются до конца приключения.", kBuilds[fz].name, r.pct);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    Cfg savedCfg = ui;
    bool lock = fz >= 0 && g_tab != 4;
    if (fz < 0 && g_tab < GRP_COUNT) {                               // строка вкладки: сколько включено и сброс только этой вкладки
        int n = cfgActiveCount(ui, g_tab);
        if (n > 0) ImGui::TextColored(kAcc, "Включено на этой вкладке: %d", n); else ImGui::TextDisabled("На этой вкладке всё как в оригинальной игре");
        ImGui::SameLine(); ImGui::BeginDisabled(!cfgGroupChanged(ui, g_tab));
        if (ImGui::SmallButton("Сбросить вкладку")) { cfgResetGroup(ui, g_tab); pushCfg(); ModSay("Настройки вкладки сброшены к оригинальным."); }
        ImGui::EndDisabled();
        ImGui::Spacing();
    }
    if (lock) { ui = buildCfg(fz); ImGui::BeginDisabled(); }
    switch (g_tab) { case 0: tabPlants(); break; case 1: tabZombies(); break; case 2: tabLevel(); break;
                     case 3: tabTools(); break; default: tabBuilds(fz); }
    if (lock) { ImGui::EndDisabled(); ui = savedCfg; }
    ImGui::EndChild();

    // статус-строка
    ImGui::Separator();
    if (!S.versionOk) ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "Версия игры не поддерживается");
    else if (S.hacksBad) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Часть патчей отключена (не совпали байты): %d", (int)S.hacksBad);
    else ImGui::TextColored(kAcc, "Подключено");
    ImGui::SameLine();
    if (S.ui >= 2) ImGui::TextDisabled("  |  Бой: волна %d/%d · солнце %d", (int)S.wave, (int)S.numWaves, (int)S.sun);
    else ImGui::TextDisabled("  |  Главное меню");
    char m[sizeof S.msg]; EnterCriticalSection(&S.lock); memcpy(m, S.msg, sizeof m); LeaveCriticalSection(&S.lock);
    ImGui::SameLine(); ImGui::TextDisabled("  |  %s", m);
    ImGui::End();
}

#ifndef PVZ_SHOT
// ---------------------------------------------------------------- DX11 и окно
static void createRtv() {
    ID3D11Texture2D* bb = nullptr;
    g_sc->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) { g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release(); }
}
static bool createDevice(HWND hw) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hw; sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; const D3D_FEATURE_LEVEL lv[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, lv, 2, D3D11_SDK_VERSION, &sd, &g_sc, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, lv, 2, D3D11_SDK_VERSION, &sd, &g_sc, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) return false;
    createRtv(); return true;
}
static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
    switch (m) {
        case WM_SIZE:
            if (g_dev && w != SIZE_MINIMIZED) {
                if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
                g_sc->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0); createRtv();
            }
            return 0;
        case WM_CLOSE: g_visible = false; ShowWindow(h, SW_HIDE); return 0;     // закрытие только прячет меню
        case WM_SYSCOMMAND: if ((w & 0xfff0) == SC_KEYMENU) return 0; break;
        case WM_KEYDOWN: if (w == VK_ESCAPE) { g_visible = false; ShowWindow(h, SW_HIDE); return 0; } break;
    }
    return DefWindowProcW(h, m, w, l);
}

static HWND findGame() {
    return findOwnGameWindow();
}

static DWORD WINAPI uiThread(LPVOID) {
    typedef BOOL (WINAPI* SetCtx)(HANDLE);
    typedef UINT (WINAPI* GetDpi)(HWND);
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (auto f = (SetCtx)GetProcAddress(u32, "SetThreadDpiAwarenessContext")) f((HANDLE)-4);   // per-monitor v2

    HMODULE selfMod = nullptr;                                   // иконка лежит в ресурсах самой DLL (id 1)
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&UiStart, &selfMod);
    HICON icoBig = selfMod ? LoadIconW(selfMod, MAKEINTRESOURCEW(1)) : nullptr;
    HICON icoSmall = selfMod ? (HICON)LoadImageW(selfMod, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0) : nullptr;
    WNDCLASSEXW wc = {sizeof wc, CS_CLASSDC, wndProc, 0, 0, GetModuleHandleW(nullptr), icoBig, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"PvZModMenu", icoSmall};
    RegisterClassExW(&wc);
    HDC dc = GetDC(nullptr); int dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc);
    SC = dpi / 96.f; if (SC < 1.f) SC = 1.f;
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int W0 = (int)(1120 * SC), H0 = (int)(780 * SC);
    W0 = std::min<int>(W0, wa.right - wa.left - 40); H0 = std::min<int>(H0, wa.bottom - wa.top - 40);
    g_menu = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"PvZModMenu", L"PvZ TBG Mod", WS_POPUP | WS_THICKFRAME,
                             wa.left + (wa.right - wa.left - W0) / 2, wa.top + (wa.bottom - wa.top - H0) / 2, W0, H0,
                             nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_menu || !createDevice(g_menu)) { ModSay("Не удалось создать окно меню (DX11)."); return 1; }
    if (auto gd = (GetDpi)GetProcAddress(u32, "GetDpiForWindow")) { SC = gd(g_menu) / 96.f; if (SC < 1.f) SC = 1.f; }

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImFont* font = nullptr;
    for (const char* p : {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf", "C:\\Windows\\Fonts\\tahoma.ttf"})
        if (!font && GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) font = io.Fonts->AddFontFromFileTTF(p, 17.f * SC, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    if (!font) io.Fonts->AddFontDefault();
    applyStyle();
    ImGui_ImplWin32_Init(g_menu); ImGui_ImplDX11_Init(g_dev, g_ctx);

    EnterCriticalSection(&S.lock); ui = S.cfg; LeaveCriticalSection(&S.lock);
    refreshProfiles();

    LONG runSaved = S.runSeq; bool prevTab = false, running = true; DWORD lastSave = GetTickCount(); std::string lastSaved;
    Cfg prev = ui;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if (msg.message == WM_QUIT) running = false; }

        bool down = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        if (down && !prevTab) {
            HWND fg = GetForegroundWindow(); HWND game = findGame();
            bool typing = g_visible && ImGui::GetIO().WantTextInput;
            if (!typing && (fg == game || fg == g_menu)) {
                g_visible = !g_visible;
                if (g_visible) { ShowWindow(g_menu, SW_SHOW); SetForegroundWindow(g_menu); }
                else { ShowWindow(g_menu, SW_HIDE); if (game) SetForegroundWindow(game); }
            }
        }
        prevTab = down;

        {   // горячие клавиши (только когда окно игры/меню в фокусе): F6 — солнце, F7 — убить зомби, F8 — скорость ×2
            static bool hk[3] = {false, false, false}; static const int vk[3] = {VK_F6, VK_F7, VK_F8};
            HWND f = GetForegroundWindow(); DWORD fpid = 0; if (f) GetWindowThreadProcessId(f, &fpid);
            bool ours = fpid == GetCurrentProcessId() && !(g_visible && ImGui::GetIO().WantTextInput);
            for (int i = 0; i < 3; i++) {
                bool d = ours && (GetAsyncKeyState(vk[i]) & 0x8000) != 0;
                if (d && !hk[i]) {
                    if (i == 0) sendCmd(CMD_GIVE_SUN);
                    else if (i == 1) sendCmd(CMD_KILL_ZOMBIES);
                    else { ui.gameSpeedOn = !ui.gameSpeedOn; if (ui.gameSpeedOn) ui.gameSpeed = 2.f; pushCfg(); ModSay(ui.gameSpeedOn ? "Скорость игры ×2 (F8)." : "Скорость игры обычная (F8)."); }
                }
                hk[i] = d;
            }
        }

        if (S.runSeq != runSaved) {                      // прохождения сборок: сохраняем даже при закрытом меню
            runSaved = S.runSeq;
            RunState tmp[kMaxRuns]; EnterCriticalSection(&S.lock); for (int i = 0; i < kMaxRuns; i++) tmp[i] = S.runs[i]; LeaveCriticalSection(&S.lock);
            writeFile(g_dataDir + L"\\runs.ini", runsToIni(tmp));
        }
        if (GetTickCount() - lastSave > 1500) {          // автосохранение last.ini
            lastSave = GetTickCount();
            std::string t = cfgToIni(ui);
            if (t != lastSaved) { writeFile(g_dataDir + L"\\last.ini", t); lastSaved = t; }
        }
        if (!g_visible) { Sleep(25); continue; }

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        drawUi();
        ImGui::Render();
        const float clear[4] = {0.067f, 0.086f, 0.110f, 1.f};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr); g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_sc->Present(1, 0);

        if (memcmp(&ui, &prev, sizeof(Cfg)) != 0) { pushCfg(); prev = ui; }
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    return 0;
}

void UiStart() { CreateThread(nullptr, 0, uiThread, nullptr, 0, nullptr); }
#endif   // PVZ_SHOT
