// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#ifndef NOMINMAX
#define NOMINMAX
#endif

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <cstdint>

// Windows Header Files (proper order):
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <oleauto.h> // For SystemTimeToVariantTime
#include <shellapi.h>
#include <process.h> // For _beginthreadex

// C++ Standard Library
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cmath> // For pow
#include <iostream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")