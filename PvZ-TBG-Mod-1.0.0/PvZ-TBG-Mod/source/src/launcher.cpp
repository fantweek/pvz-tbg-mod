// Лаунчер PvZ TBG Mod: запускает игру (или находит уже запущенную) и подключает pvzmod.dll.
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <cstring>

static std::wstring dirOf(const std::wstring& p) { return p.substr(0, p.find_last_of(L'\\')); }
static bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

static std::wstring regSteamPath() {
    wchar_t buf[MAX_PATH]; DWORD n = sizeof buf; HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, L"SteamPath", nullptr, nullptr, (LPBYTE)buf, &n) == ERROR_SUCCESS) { RegCloseKey(k); std::wstring s = buf; for (auto& c : s) if (c == L'/') c = L'\\'; return s; }
        RegCloseKey(k);
    }
    return L"";
}
static std::wstring findGameExe(const std::wstring& here) {
    if (exists(here + L"\\PlantsVsZombies.exe")) return here + L"\\PlantsVsZombies.exe";
    std::wstring steam = regSteamPath();
    const wchar_t* names[] = {L"Plants vs Zombies GOTY Edition", L"Plants vs. Zombies GOTY Edition", L"Plants vs Zombies"};
    std::wstring roots[] = {steam, L"C:\\Program Files (x86)\\Steam", L"C:\\Program Files\\Steam", L"D:\\Steam", L"D:\\SteamLibrary", L"E:\\SteamLibrary"};
    for (auto& r : roots) for (auto n : names) {
        std::wstring p = r + L"\\steamapps\\common\\" + n + L"\\PlantsVsZombies.exe";
        if (!r.empty() && exists(p)) return p;
    }
    return L"";
}
static std::wstring procName(DWORD pid) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return L"(процесс не виден)";
    wchar_t b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; bool ok = QueryFullProcessImageNameW(p, 0, b, &n) != 0; CloseHandle(p);
    if (!ok) return L"?";
    std::wstring s = b; size_t k = s.find_last_of(L'\\');
    return k == std::wstring::npos ? s : s.substr(k + 1);
}
// Окно игры: класс «MainWindow» (PopCap) с заголовком «Plants vs. Zombies». Имя exe не проверяем: у модифицированных сборок оно бывает другим.
// diag, если задан, получает список всех найденных окон этого класса (для сообщения об ошибке).
static DWORD findPvz(std::wstring* diag = nullptr) {
    DWORD found = 0; HWND w = nullptr;
    while ((w = FindWindowExA(nullptr, w, "MainWindow", nullptr)) != nullptr) {
        char t[160] = {0}; GetWindowTextA(w, t, 159);
        DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
        if (diag) {
            wchar_t wt[160] = {0}; MultiByteToWideChar(CP_ACP, 0, t, -1, wt, 159);
            *diag += L"\n  «" + std::wstring(wt) + L"» — " + procName(pid) + (IsWindowVisible(w) ? L"" : L" (скрыто)");
        }
        if (!found && pid && strstr(t, "Plants") && IsWindowVisible(w)) found = pid;
    }
    return found;
}
// Уже есть процесс PlantsVsZombies.exe (даже если окна ещё нет — игра грузится)? Второй экземпляр запускать нельзя:
// он упадёт с «Unable to load Steam.dll».
static bool pvzProcessRunning() {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = {sizeof pe}; bool found = false;
    for (BOOL ok = Process32FirstW(s, &pe); ok; ok = Process32NextW(s, &pe)) if (!_wcsicmp(pe.szExeFile, L"PlantsVsZombies.exe") || !_wcsicmp(pe.szExeFile, L"popcapgame1.exe")) found = true;
    CloseHandle(s); return found;
}
static bool alreadyInjected(DWORD pid) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me = {sizeof me}; bool found = false;
    for (BOOL ok = Module32FirstW(s, &me); ok; ok = Module32NextW(s, &me)) if (!_wcsicmp(me.szModule, L"pvzmod.dll")) found = true;
    CloseHandle(s); return found;
}
static bool inject(DWORD pid, const std::wstring& dll) {
    HANDLE p = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!p) return false;
    size_t sz = (dll.size() + 1) * sizeof(wchar_t);
    void* mem = VirtualAllocEx(p, nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool ok = false;
    if (mem && WriteProcessMemory(p, mem, dll.c_str(), sz, nullptr)) {
        HANDLE t = CreateRemoteThread(p, nullptr, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"), mem, 0, nullptr);
        if (t) {
            DWORD w = WaitForSingleObject(t, 10000); DWORD code = 0; GetExitCodeThread(t, &code);
            ok = w == WAIT_OBJECT_0 && code != 0;
            if (w == WAIT_TIMEOUT) mem = nullptr;               // поток ещё может читать путь — память не освобождаем
            CloseHandle(t);
        }
    }
    if (mem) VirtualFreeEx(p, mem, 0, MEM_RELEASE);
    CloseHandle(p); return ok;
}


// ---------------------------------------------------------------- автозапуск
static const wchar_t* RUNKEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static bool autoStartOn() {
    HKEY k; bool on = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUNKEY, 0, KEY_READ, &k) == ERROR_SUCCESS) { on = RegQueryValueExW(k, L"PvZMod", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; RegCloseKey(k); }
    return on;
}
static void setAutoStart(bool on, const std::wstring& self) {
    HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, RUNKEY, 0, KEY_WRITE, &k) != ERROR_SUCCESS) return;
    if (on) { std::wstring v = L"\"" + self + L"\" --watch"; RegSetValueExW(k, L"PvZMod", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t))); }
    else RegDeleteValueW(k, L"PvZMod");
    RegCloseKey(k);
}
static bool askedBefore() {
    HKEY k; DWORD v = 0, n = sizeof v; bool r = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PvZMod", 0, KEY_READ, &k) == ERROR_SUCCESS) { r = RegQueryValueExW(k, L"AskedAutostart", nullptr, nullptr, (LPBYTE)&v, &n) == ERROR_SUCCESS && v; RegCloseKey(k); }
    return r;
}
static void setAsked() {
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\PvZMod", 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExW(k, L"AskedAutostart", 0, REG_DWORD, (const BYTE*)&v, sizeof v); RegCloseKey(k);
    }
}
static void startWatcher(const std::wstring& self) {
    std::wstring cmd = L"\"" + self + L"\" --watch";
    STARTUPINFOW si = {sizeof si}; PROCESS_INFORMATION pi = {};
    std::wstring dir = dirOf(self);
    if (CreateProcessW(self.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}

// ---------------------------------------------------------------- режим «следить» (значок в трее)
struct Watch {
    HWND hwnd = nullptr; DWORD handled = 0, seenPid = 0, seenAt = 0; UINT taskbarMsg = 0; int tries = 0;
    std::wstring self, dll; NOTIFYICONDATAW nid = {};
} W;
enum { WM_TRAY = WM_APP + 1, ID_AUTO = 1, ID_NOW = 2, ID_EXIT = 3, TIMER_POLL = 1 };

static void trayAdd() {
    W.nid = {}; W.nid.cbSize = sizeof W.nid; W.nid.hWnd = W.hwnd; W.nid.uID = 1;
    W.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; W.nid.uCallbackMessage = WM_TRAY;
    W.nid.hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!W.nid.hIcon) W.nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy(W.nid.szTip, L"PvZ TBG Mod: жду запуска игры");
    Shell_NotifyIconW(NIM_ADD, &W.nid);
}
static void trayBalloon(const wchar_t* text) {
    NOTIFYICONDATAW n = W.nid; n.uFlags = NIF_INFO; n.dwInfoFlags = NIIF_INFO;
    wcsncpy(n.szInfoTitle, L"PvZ TBG Mod", 63); wcsncpy(n.szInfo, text, 255);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}
static void watchTry(bool now) {
    DWORD pid = findPvz();
    if (!pid) { W.seenPid = 0; return; }
    if (pid == W.handled && !now) return;
    if (pid != W.seenPid) { W.seenPid = pid; W.seenAt = GetTickCount(); W.tries = 0; if (!now) return; }
    if (!now && GetTickCount() - W.seenAt < 1200) return;      // даём игре дорисовать окно
    if (alreadyInjected(pid)) { W.handled = pid; if (now) trayBalloon(L"Мод уже подключён. В игре нажмите Tab."); return; }
    bool ok = inject(pid, W.dll);
    if (ok) { W.handled = pid; W.tries = 0; trayBalloon(L"Мод подключён. В игре нажмите Tab."); return; }
    // неудача (например, игра ещё грузится): следящий режим повторит попытку на следующем опросе, но не больше 4 раз
    if (++W.tries >= 4 || now) {
        W.handled = pid; W.tries = 0;
        trayBalloon(L"Не удалось подключить мод. Если игра запущена от администратора, запустите лаунчер так же.");
    }
}
static LRESULT CALLBACK watchProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: SetTimer(h, TIMER_POLL, 700, nullptr); return 0;
    case WM_TIMER: if (wp == TIMER_POLL) watchTry(false); return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (autoStartOn() ? MF_CHECKED : 0), ID_AUTO, L"Запускаться вместе с Windows");
            AppendMenuW(menu, MF_STRING, ID_NOW, L"Подключить к игре сейчас");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, ID_EXIT, L"Выход (мод в уже запущенной игре останется)");
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(h);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
            PostMessageW(h, WM_NULL, 0, 0); DestroyMenu(menu);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_AUTO) setAutoStart(!autoStartOn(), W.self);
        else if (LOWORD(wp) == ID_NOW) watchTry(true);
        else if (LOWORD(wp) == ID_EXIT) DestroyWindow(h);
        return 0;
    case WM_DESTROY: Shell_NotifyIconW(NIM_DELETE, &W.nid); PostQuitMessage(0); return 0;
    }
    if (m == W.taskbarMsg && m) { trayAdd(); return 0; }       // проводник перезапущен — вернуть значок
    return DefWindowProcW(h, m, wp, lp);
}
static int runWatcher(const std::wstring& self, const std::wstring& dll) {
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\PvZModWatcher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;       // уже следит
    W.self = self; W.dll = dll; W.taskbarMsg = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc = {}; wc.lpfnWndProc = watchProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"PvZModWatcher";
    RegisterClassW(&wc);
    W.hwnd = CreateWindowExW(0, wc.lpszClassName, L"PvZ TBG Mod", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!W.hwnd) return 1;
    trayAdd();
    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (mtx) CloseHandle(mtx);
    return 0;
}

// ---------------------------------------------------------------- вход
// Режимы:
//   PvZModLauncher.exe                — запустить игру (или найти запущенную) и подключить мод;
//   PvZModLauncher.exe --watch        — фоном ждать запуска игры (из Steam или откуда угодно) и подключать мод сам;
//   PvZModLauncher.exe --autostart on|off — включить/выключить запуск слежения вместе с Windows;
//   "PvZModLauncher.exe" %command%    — параметры запуска игры в Steam (запускает игру и подключает мод).
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t selfBuf[MAX_PATH]; GetModuleFileNameW(nullptr, selfBuf, MAX_PATH);
    std::wstring self = selfBuf, here = dirOf(self), dll = here + L"\\pvzmod.dll";
    if (!exists(dll)) { MessageBoxW(nullptr, L"Рядом с лаунчером не найден pvzmod.dll.", L"PvZ TBG Mod", MB_ICONERROR); return 1; }

    int argc = 0; wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring a1 = argc > 1 ? argv[1] : L"";
    if (a1 == L"--watch") return runWatcher(self, dll);
    if (a1 == L"--autostart") {
        bool on = argc > 2 && std::wstring(argv[2]) == L"on";
        setAutoStart(on, self);
        if (on) startWatcher(self);
        return 0;
    }
    bool steamMode = argc > 1 && a1.size() > 4 && !_wcsicmp(a1.c_str() + a1.size() - 4, L".exe") && exists(a1);

    DWORD pid = findPvz();
    bool waited = false;
    if (!pid && !steamMode && pvzProcessRunning()) {            // игра уже стартует (например, из Steam) — просто ждём её окно
        waited = true;
        for (int i = 0; i < 300 && !(pid = findPvz()); i++) Sleep(100);
        Sleep(800);
    }
    if (!pid && !waited) {
        std::wstring exe = steamMode ? a1 : findGameExe(here);
        bool viaSteam = !steamMode && exe.find(L"\\steamapps\\") != std::wstring::npos;
        if (viaSteam) {                                          // как кнопка «Играть»: игра стартует в окружении Steam
            ShellExecuteW(nullptr, L"open", L"steam://rungameid/3590", nullptr, nullptr, SW_SHOWNORMAL);
            for (int i = 0; i < 900 && !(pid = findPvz()); i++) Sleep(100);   // Steam может стартовать долго: ждём до 90 с
            Sleep(800);
        } else
        if (exe.empty()) { MessageBoxW(nullptr, L"Не нашёл PlantsVsZombies.exe.\nПоложите лаунчер и pvzmod.dll в папку с игрой.", L"PvZ TBG Mod", MB_ICONERROR); return 1; }
        if (!viaSteam) {
        std::wstring cmd = L"\"" + exe + L"\"";
        if (steamMode) for (int i = 2; i < argc; i++) cmd += L" \"" + std::wstring(argv[i]) + L"\"";   // остальные параметры Steam
        STARTUPINFOW si = {sizeof si}; PROCESS_INFORMATION pi = {};
        std::wstring dir = dirOf(exe);
        if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
            MessageBoxW(nullptr, L"Не удалось запустить игру.", L"PvZ TBG Mod", MB_ICONERROR); return 1;
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        for (int i = 0; i < 600 && !(pid = findPvz()); i++) Sleep(100);     // ждём окно игры до 60 с
        Sleep(800);
        }
    }
    if (!pid) {
        std::wstring diag; findPvz(&diag);
        MessageBoxW(nullptr, (L"Окно игры не найдено.\nНайденные окна класса MainWindow:" + (diag.empty() ? std::wstring(L" нет") : diag)).c_str(), L"PvZ TBG Mod", MB_ICONERROR);
        return 1;
    }
    if (alreadyInjected(pid)) { if (!steamMode) MessageBoxW(nullptr, L"Мод уже подключён. В игре нажмите Tab.", L"PvZ TBG Mod", MB_ICONINFORMATION); return 0; }
    if (!inject(pid, dll)) { MessageBoxW(nullptr, L"Не удалось подключить мод. Запустите лаунчер от имени администратора, если игра запущена так же.", L"PvZ TBG Mod", MB_ICONERROR); return 1; }

    if (!steamMode && !autoStartOn() && !askedBefore()) {                    // один раз предлагаем автоподключение
        setAsked();
        if (MessageBoxW(nullptr,
                L"Мод подключён (в игре нажмите Tab).\n\nПодключать мод автоматически при каждом запуске игры, в том числе кнопкой «Играть» в Steam?\n"
                L"Лаунчер будет тихо работать в фоне со значком в трее и запускаться вместе с Windows. Отключить можно в меню значка.",
                L"PvZ TBG Mod", MB_YESNO | MB_ICONQUESTION) == IDYES) { setAutoStart(true, self); startWatcher(self); }
    }
    return 0;
}
