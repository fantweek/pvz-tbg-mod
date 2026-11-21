// Минимальная заглушка WinAPI для запуска логики мода на Linux (только тесты).
#pragma once
#include <stdint.h>
#include <string.h>
#include <time.h>
typedef unsigned long DWORD; typedef long LONG; typedef int BOOL; typedef void* LPVOID; typedef void* HWND;
typedef long long LONGLONG; typedef uintptr_t WPARAM; typedef intptr_t LPARAM;
struct CRITICAL_SECTION { int x; };
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION*) {}
inline void LeaveCriticalSection(CRITICAL_SECTION*) {}
inline LONG InterlockedExchange(volatile LONG* p, LONG v) { LONG o = *p; *p = v; return o; }
inline LONG InterlockedIncrement(volatile LONG* p) { return ++*p; }
inline BOOL IsBadReadPtr(const void*, size_t) { return 0; }
inline DWORD GetTickCount() { return 1000; }
union LARGE_INTEGER { long long QuadPart; };
inline void QueryPerformanceCounter(LARGE_INTEGER*) {}
inline void QueryPerformanceFrequency(LARGE_INTEGER*) {}
struct RECT { long left, top, right, bottom; };
// --- добавлено: то, что использует mod.cpp (патчи кода и клик по меню) ---
#define PAGE_EXECUTE_READWRITE 0x40
inline BOOL VirtualProtect(void*, size_t, DWORD, DWORD* o) { if (o) *o = 0; return 1; }
inline void* GetCurrentProcess() { return nullptr; }
inline BOOL FlushInstructionCache(void*, const void*, size_t) { return 1; }
inline BOOL GetClientRect(HWND, RECT* r) { r->left = r->top = 0; r->right = 800; r->bottom = 600; return 1; }
inline BOOL PostMessageW(HWND, unsigned, WPARAM, LPARAM) { return 1; }
#define MAKELPARAM(l, h) ((LPARAM)(((unsigned short)(l)) | ((unsigned)((unsigned short)(h)) << 16)))
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define MK_LBUTTON 0x0001
