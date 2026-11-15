// Выбор платформенных заголовков: настоящий Windows или тестовая заглушка (Linux, 32 бита).
#pragma once
#ifdef PVZ_TEST
#include "winshim.h"
#else
#include <windows.h>
#endif
