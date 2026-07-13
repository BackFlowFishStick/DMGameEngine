/*
 * DMGameEngine - Core Export Macros
 *
 * Usage:
 *   - Define DMGE_BUILD_DLL when building the engine (handled by CMake).
 *   - Game projects consuming the DLL will see DMGE_API as __declspec(dllimport).
 */

#pragma once
#include <memory>
#include <utility>

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

namespace DM
{
    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename... Args>
    Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
}

// // Bring aliases into DMGameEngine namespace for convenience
// namespace DMGameEngine
// {
//     using DM::Scope;
//     using DM::Ref;
//     using DM::CreateScope;
//     using DM::CreateRef;
// }