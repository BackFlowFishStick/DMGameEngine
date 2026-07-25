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

    // Request the swapchain be recreated at the next BeginFrame. Default
    // no-op for backends without an out-of-date swapchain (e.g. OpenGL, whose
    // default framebuffer is implicitly resized by the driver). The Vulkan
    // backend overrides this to set its resize flag so BeginFrame recreates
    // the swapchain immediately instead of waiting for vkAcquireNextImageKHR
    // to return OUT_OF_DATE (which leaves 1..N frames with viewport/extent
    // mismatch -> flicker/clipping right after a window resize).
    virtual void RequestResize(uint32_t /*width*/, uint32_t /*height*/) {}

    virtual const char* GetVendor() const = 0;
    virtual const char* GetRenderer() const = 0;
    virtual const char* GetVersion() const = 0;
};

} // namespace DMGameEngine
