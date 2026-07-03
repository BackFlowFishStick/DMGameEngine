/*
 * DMGameEngine - Shader Abstraction
 *
 * Base class for all graphics API shader implementations.
 * Platform backends (OpenGL, Vulkan, DirectX) derive from this
 * and provide their own compilation, binding and uniform handling.
 *
 * Shaders are created via the static Create() factory, which
 * selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "glm/glm.hpp"
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <cstdint>
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

// ── Shader Data Types ──────────────────────────────────────────────

enum class ShaderDataType : uint8_t
{
    None    = 0,
    Float,  Float2, Float3, Float4,
    Int,    Int2,   Int3,   Int4,
    Bool,
    Mat3,   Mat4
};

inline uint32_t ShaderDataTypeSize(ShaderDataType type)
{
    switch (type)
    {
        case ShaderDataType::Float:    return 4;
        case ShaderDataType::Float2:   return 4 * 2;
        case ShaderDataType::Float3:   return 4 * 3;
        case ShaderDataType::Float4:   return 4 * 4;
        case ShaderDataType::Int:      return 4;
        case ShaderDataType::Int2:     return 4 * 2;
        case ShaderDataType::Int3:     return 4 * 3;
        case ShaderDataType::Int4:     return 4 * 4;
        case ShaderDataType::Bool:     return 1;
        case ShaderDataType::Mat3:     return 4 * 3 * 3;
        case ShaderDataType::Mat4:     return 4 * 4 * 4;
        default:                       return 0;
    }
}

// ── Buffer Element ─────────────────────────────────────────────────

struct DMGE_API BufferElement
{
    std::string   Name;
    ShaderDataType Type;
    uint32_t       Size;
    uint32_t       Offset;
    bool           Normalized;

    BufferElement() = default;

    BufferElement(ShaderDataType type, std::string_view name, bool normalized = false)
        : Name(name)
        , Type(type)
        , Size(ShaderDataTypeSize(type))
        , Offset(0)
        , Normalized(normalized)
    {
    }

    uint32_t GetComponentCount() const
    {
        switch (Type)
        {
            case ShaderDataType::Float:  return 1;
            case ShaderDataType::Float2: return 2;
            case ShaderDataType::Float3: return 3;
            case ShaderDataType::Float4: return 4;
            case ShaderDataType::Int:    return 1;
            case ShaderDataType::Int2:   return 2;
            case ShaderDataType::Int3:   return 3;
            case ShaderDataType::Int4:   return 4;
            case ShaderDataType::Bool:   return 1;
            case ShaderDataType::Mat3:   return 3 * 3;
            case ShaderDataType::Mat4:   return 4 * 4;
            default:                     return 0;
        }
    }
};

// ── Buffer Layout ──────────────────────────────────────────────────

class DMGE_API BufferLayout
{
public:
    BufferLayout() = default;

    BufferLayout(std::initializer_list<BufferElement> elements)
        : m_Elements(elements)
    {
        CalculateOffsetsAndStride();
    }

    const std::vector<BufferElement>& GetElements() const { return m_Elements; }
    uint32_t GetStride() const { return m_Stride; }

    auto begin()       { return m_Elements.begin(); }
    auto end()         { return m_Elements.end();   }
    auto begin() const { return m_Elements.begin(); }
    auto end()   const { return m_Elements.end();   }

private:
    void CalculateOffsetsAndStride()
    {
        uint32_t offset = 0;
        m_Stride = 0;
        for (auto& element : m_Elements)
        {
            element.Offset = offset;
            offset += element.Size;
            m_Stride += element.Size;
        }
    }

    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;
};

// ── Shader ─────────────────────────────────────────────────────────

class DMGE_API Shader
{
public:
    virtual ~Shader() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;

    virtual const std::string& GetName() const = 0;

    // ── Uniform setters ─────────────────────────────────────────
    virtual void SetInt(std::string_view name, int value)                  = 0;
    virtual void SetIntArray(std::string_view name, const int* values, uint32_t count) = 0;
    virtual void SetFloat(std::string_view name, float value)              = 0;
    virtual void SetFloat2(std::string_view name, const glm::vec2& value)  = 0;
    virtual void SetFloat3(std::string_view name, const glm::vec3& value)  = 0;
    virtual void SetFloat4(std::string_view name, const glm::vec4& value)  = 0;
    virtual void SetMat4(std::string_view name, const glm::mat4& value)    = 0;

    // ── Factory ─────────────────────────────────────────────────
    static std::shared_ptr<Shader> Create(std::string_view filepath);
    static std::shared_ptr<Shader> Create(std::string_view name,
                                          std::string_view vertexSrc,
                                          std::string_view fragmentSrc);
};

} // namespace DMGameEngine
