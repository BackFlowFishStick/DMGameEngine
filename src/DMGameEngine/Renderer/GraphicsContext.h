/*
 * DMGameEngine - Graphics Context Abstraction
 *
 * Platform-agnostic interface for a rendering context.
 * Platform backends (OpenGL, Vulkan, DirectX) implement this interface.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

namespace DMGameEngine {

class DMGE_API GraphicsContext
{
public:
    virtual ~GraphicsContext() = default;

    virtual void Init() = 0;
    virtual void SwapBuffers() = 0;

    virtual const char* GetVendor() const = 0;
    virtual const char* GetRenderer() const = 0;
    virtual const char* GetVersion() const = 0;
};

} // namespace DMGameEngine
