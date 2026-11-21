// Заглушки WinAPI для сборки ui.cpp на Linux (только для скриншотов README). Ничего из этого не выполняется:
// рисуется только drawUi(), а окно, DX11 и файловые операции в этом режиме не нужны.
#pragma once
#define PVZ_TEST 1
#include "../../src/plat.h"
#include <string>
typedef void* HANDLE; typedef unsigned int UINT; typedef unsigned short WORD; typedef wchar_t WCHAR;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define CP_UTF8 65001
#define GENERIC_WRITE 0x40000000
#define GENERIC_READ 0x80000000
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_SHARE_READ 1
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 8
#define WM_NCLBUTTONDOWN 0xA1
#define HTCAPTION 2
#define SW_HIDE 0
struct WIN32_FIND_DATAW { WCHAR cFileName[260]; };
inline int MultiByteToWideChar(UINT, DWORD, const char*, int, wchar_t*, int) { return 1; }
inline int WideCharToMultiByte(UINT, DWORD, const wchar_t*, int, char*, int, const char*, BOOL*) { return 1; }
inline HANDLE CreateFileW(const wchar_t*, DWORD, DWORD, void*, DWORD, DWORD, void*) { return INVALID_HANDLE_VALUE; }
inline BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*) { return 0; }
inline BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, void*) { return 0; }
inline BOOL CloseHandle(HANDLE) { return 1; }
inline BOOL DeleteFileW(const wchar_t*) { return 1; }
inline BOOL MoveFileExW(const wchar_t*, const wchar_t*, DWORD) { return 0; }
inline HANDLE FindFirstFileW(const wchar_t*, WIN32_FIND_DATAW*) { return INVALID_HANDLE_VALUE; }
inline BOOL FindNextFileW(HANDLE, WIN32_FIND_DATAW*) { return 0; }
inline BOOL FindClose(HANDLE) { return 1; }
inline BOOL ReleaseCapture() { return 1; }
inline long SendMessageW(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline BOOL ShowWindow(HWND, int) { return 1; }
