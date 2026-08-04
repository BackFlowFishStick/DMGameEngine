/*
 * DMGameEngine - Vulkan Shader
 *
 * Vulkan implementation of the Shader abstraction.
 *
 * Because Vulkan consumes SPIR-V (not GLSL source) and has no
 * name-based uniforms, this backend:
 *   1. Reads the same `#type vertex` / `#type fragment` GLSL files as
 *      the OpenGL backend.
 *   2. Rewrites the GLSL for Vulkan at load time:
 *        - bumps the version to 450 core,
 *        - collects standalone `uniform <type> <name>;` declarations into a
 *          single synthesized `layout(std140, set=0, binding=0)` UBO block
 *          (so the exact byte offsets are known without SPIR-V reflection),
 *        - assigns explicit descriptor bindings to `uniform sampler*` decls,
 *        - injects explicit `layout(location=...)` qualifiers where missing.
 *   3. Compiles each stage to SPIR-V at runtime via shaderc.
 *   4. Builds the VkShaderModules, VkDescriptorSetLayout and VkPipelineLayout.
 *
 * Uniforms are uploaded by name into a host staging buffer (std140 layout)
 * via the Set*() setters. At draw time VulkanRendererAPI commits the
 * staging buffer into a per-frame dynamic UBO slot, reproducing the
 * OpenGL "set uniform on the bound program" model.
 */

#pragma once

#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDebug.h"

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace DMGameEngine {

class VulkanShader : public Shader
{
public:
    VulkanShader(std::string_view name, std::string_view vertexSrc, std::string_view fragmentSrc);
    explicit VulkanShader(std::string_view filepath);
    ~VulkanShader() override;

    void Bind()   const override;
    void Unbind() const override;

    const std::string& GetName() const override { return m_Name; }

    // ── Uniform setters (write into the std140 staging buffer) ──
    void SetInt(std::string_view name, int value) override;
    void SetIntArray(std::string_view name, const int* values, uint32_t count) override;
    void SetFloat(std::string_view name, float value) override;
    void SetFloat2(std::string_view name, const glm::vec2& value) override;
    void SetFloat3(std::string_view name, const glm::vec3& value) override;
    void SetFloat4(std::string_view name, const glm::vec4& value) override;
    void SetMat4(std::string_view name, const glm::mat4& value) override;

    // ── Backend interop ─────────────────────────────────────────
    uint64_t GetID() const { return m_ID; }

    uint32_t     GetUniformBlockSize()   const { return m_UniformBlockSize; }
    const void*  GetUniformData()        const { return m_UniformData.data(); }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
    VkPipelineLayout      GetPipelineLayout()      const { return m_PipelineLayout; }
    const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStages() const { return m_Stages; }
    uint32_t     GetSamplerCount()       const { return static_cast<uint32_t>(m_Samplers.size()); }

private:
    struct UniformMember
    {
        std::string name;
        std::string glslType;
        uint32_t    offset      = 0;
        uint32_t    size        = 0; // total bytes
        uint32_t    elementSize = 0; // per-element stride (arrays)
        uint32_t    count       = 1;
    };
    struct SamplerDecl
    {
        std::string name;
        std::string glslType;
        uint32_t    binding = 0;
    };

    std::string ReadFile(std::string_view filepath) const;
    std::unordered_map<uint32_t, std::string> PreProcess(std::string_view source) const;

    // GLSL rewriting / collection.
    void CollectDeclarations(const std::string& source);
    void LayoutUniforms();
    std::string BuildUBOBlock() const;
    std::string BuildSamplerDecls() const;
    std::string RewriteStageBody(const std::string& source, bool isVertex) const;

    std::vector<uint32_t> CompileStage(const std::string& source,
                                       shaderc_shader_kind kind,
                                       const char* stageName) const;
    VkShaderModule CreateModule(const std::vector<uint32_t>& code) const;
    void BuildShaderStages();
    void CreatePipelineLayout();
    void DestroyModules();

    void WriteUniform(std::string_view name, const void* data, uint32_t size);
    void WriteUniformArray(std::string_view name, const int* values, uint32_t count);

    uint64_t m_ID = 0;
    static uint64_t s_NextID;

    std::string m_Name;
    std::string m_FilePath;

    VkShaderModule m_VertModule = VK_NULL_HANDLE;
    VkShaderModule m_FragModule = VK_NULL_HANDLE;
    std::vector<VkPipelineShaderStageCreateInfo> m_Stages;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_PipelineLayout      = VK_NULL_HANDLE;

    std::vector<UniformMember> m_UniformMembers;
    std::unordered_map<std::string, size_t> m_UniformIndex;
    std::vector<SamplerDecl>  m_Samplers;
    std::unordered_map<std::string, uint32_t> m_SamplerBindings;

    std::vector<uint8_t> m_UniformData;
    uint32_t m_UniformBlockSize = 0;
};

} // namespace DMGameEngine
