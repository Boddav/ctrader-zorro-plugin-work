#pragma once

// Minimal Zorro API header stub for building the plugin
// This stub defines only what is required by ctrader_plugin.cpp to compile.

#ifndef ZORRO_MIN_STUB
#define ZORRO_MIN_STUB

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <oleauto.h>
#endif

// Use plain cdecl exports
#ifndef DLLFUNC
  #ifdef _MSC_VER
    #define DLLFUNC extern "C" __declspec(dllexport)
  #else
    #define DLLFUNC extern "C"
  #endif
#endif

// DATE is provided by <oleauto.h> on Windows. Provide a fallback for safety.
#ifndef _WIN32
  typedef double DATE;
#endif

// T6 struct for tick data
typedef struct T6 {
    DATE time;
    double fOpen;
    double fHigh;
    double fLow;
    double fClose;
    double fVal;
} T6;

// Broker function prototypes used by Zorro
extern int(__cdecl* BrokerMessage)(const char* Text);
extern int(__cdecl* BrokerProgress)(intptr_t Progress);

#endif // ZORRO_MIN_STUB