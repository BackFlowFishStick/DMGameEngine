/*
 * DMGameEngine - Core Export Macros
 *
 * Usage:
 *   - Define DMGE_BUILD_DLL when building the engine (handled by CMake).
 *   - Game projects consuming the DLL will see DMGE_API as __declspec(dllimport).
 */

#pragma once

#if defined(DMGE_BUILD_DLL)
    // Building the engine DLL
    #if defined(_WIN32) || defined(_WIN64)
        #define DMGE_API __declspec(dllexport)
    #else
        #define DMGE_API __attribute__((visibility("default")))
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    // Consuming the engine DLL on Windows
    #define DMGE_API __declspec(dllimport)
#else
    // Static build or unsupported platform — no export/import needed
    #define DMGE_API
#endif
