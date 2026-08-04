/*
 * DMGameEngine - OpenGL Debug Helpers
 *
 * Debug-only OpenGL error checking. DMGE_GL_CALL wraps a single OpenGL
 * call so that any GL error it raises is reported with the offending
 * call text, source file and line, then trips the engine assert
 * (DMGE_CORE_ASSERT -> critical log + __debugbreak) at the call site.
 *
 * Usage:
 *   DMGE_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_RendererID));
 *   DMGE_GL_CALL(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr));
 *
 * Wrap only fire-and-forget calls. Do NOT wrap calls whose return value
 * is used (glCreate*, glGetString, glGetUniformLocation, glGetShaderiv,
 * glGetProgramiv, glGet*InfoLog) - wrap nothing whose result you need.
 *
 * Zero overhead in release: when DMGE_ENABLE_ASSERTS is undefined,
 * DMGE_GL_CALL(x) expands to just x.
 */

#pragma once

#include "DMGameEngine/Core/Log.h"   // DMGE_LOG_*, DMGE_CORE_ASSERT, DMGE_ENABLE_ASSERTS

#include <glad/glad.h>                // GLenum, glGetError, GL_* error codes

namespace DMGameEngine::Detail {

#ifdef DMGE_ENABLE_ASSERTS

// Drain pending GL errors so the next check reflects only this call.
inline void GLClearErrors()
{
    while (glGetError() != GL_NO_ERROR) { }
}

// Human-readable name for a GL error code.
inline const char* GLDecodeError(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:                      return "GL_NO_ERROR";
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:                               return "UNKNOWN_GL_ERROR";
    }
}

// Report every pending GL error against the call that raised it.
// Returns true when no error occurred. The caller (DMGE_GL_CALL) turns
// a false result into DMGE_CORE_ASSERT so the break lands at the call site.
inline bool GLCheckErrors(const char* glCall, const char* file, int line)
{
    GLenum error = glGetError();
    bool noError = true;
    while (error != GL_NO_ERROR)
    {
        noError = false;
        DMGE_LOG_ERROR("[OpenGL Error] {} (0x{:04X}) in: {}\n    @ {}:{}",
                       GLDecodeError(error), error, glCall, file, line);
        error = glGetError();
    }
    return noError;
}

#endif // DMGE_ENABLE_ASSERTS

} // namespace DMGameEngine::Detail

// ── GL call wrapper ──────────────────────────────────────────────
#ifdef DMGE_ENABLE_ASSERTS
    #define DMGE_GL_CALL(x) \
        do { \
            ::DMGameEngine::Detail::GLClearErrors(); \
            x; \
            DMGE_CORE_ASSERT(::DMGameEngine::Detail::GLCheckErrors(#x, __FILE__, __LINE__), \
                             "OpenGL error(s) detected in: {}", #x); \
        } while (false)
#else
    #define DMGE_GL_CALL(x) x
#endif