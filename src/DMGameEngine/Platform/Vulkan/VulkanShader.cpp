/*
 * DMGameEngine - Vulkan Shader Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanShader.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"

#include "DMGameEngine/Core/Log.h"

#include <glm/glm.hpp>

#include <fstream>
#include <sstream>
#include <regex>
#include <cstring>
#include <algorithm>

namespace DMGameEngine {

uint64_t VulkanShader::s_NextID = 1;

namespace {

// Stage indices (used as map keys).
constexpr uint32_t kStageVertex   = 0;
constexpr uint32_t kStageFragment = 1;

uint32_t ShaderTypeTokenToStage(std::string_view token)
{
    if (token == "vertex")                          return kStageVertex;
    if (token == "fragment" || token == "pixel")    return kStageFragment;
    return ~0u;
}

std::string StripComment(std::string_view line)
{
    auto pos = line.find("//");
    if (pos != std::string_view::npos)
        return std::string(line.substr(0, pos));
    return std::string(line);
}

// std140 alignment + size for a scalar/vector/matrix GLSL type.
struct Std140Layout { uint32_t align; uint32_t size; };
Std140Layout TypeStd140Layout(const std::string& type)
{
    if (type == "float" || type == "int" || type == "uint" || type == "bool")
        return { 4, 4 };
    if (type == "vec2" || type == "ivec2" || type == "uvec2")
        return { 8, 8 };
    if (type == "vec3" || type == "ivec3" || type == "uvec3")
        return { 16, 12 };
    if (type == "vec4" || type == "ivec4" || type == "uvec4")
        return { 16, 16 };
    if (type == "mat4" || type == "mat4x4")
        return { 16, 64 };
    if (type == "mat3" || type == "mat3x3")
        return { 16, 48 };
    if (type == "mat2")
        return { 16, 32 };
    return { 16, 16 };
}

uint32_t RoundUp(uint32_t v, uint32_t a)
{
    if (a == 0) return v;
    return (v + a - 1) & ~(a - 1);
}

bool StartsWith(const std::string& s, const char* prefix)
{
    return s.rfind(prefix, 0) == 0;
}

} // anonymous namespace

// ── Constructors / Destructor ──────────────────────────────────────

VulkanShader::VulkanShader(std::string_view name,
                           std::string_view vertexSrc,
                           std::string_view fragmentSrc)
    : m_Name(name), m_ID(s_NextID++)
{
    std::unordered_map<uint32_t, std::string> sources;
    sources[kStageVertex]   = std::string(vertexSrc);
    sources[kStageFragment] = std::string(fragmentSrc);

    // Pass 1: collect every uniform/sampler across all stages so the
    // synthesized UBO block + sampler bindings are identical per stage.
    for (const auto& [stage, src] : sources)
        CollectDeclarations(src);
    LayoutUniforms();

    const std::string uboBlock    = BuildUBOBlock();
    const std::string samplerDecls = BuildSamplerDecls();

    // Pass 2: rewrite each stage body and compile.
    for (const auto& [stage, src] : sources)
    {
        bool isVertex = (stage == kStageVertex);
        std::string body = RewriteStageBody(src, isVertex);
        std::string finalSrc = "#version 450 core\n" + uboBlock + samplerDecls + body;

        if (stage == kStageVertex)
        {
            auto spv = CompileStage(finalSrc, shaderc_vertex_shader, "vertex");
            m_VertModule = CreateModule(spv);
        }
        else
        {
            auto spv = CompileStage(finalSrc, shaderc_fragment_shader, "fragment");
            m_FragModule = CreateModule(spv);
        }
    }

    BuildShaderStages();
    CreatePipelineLayout();
}

VulkanShader::VulkanShader(std::string_view filepath)
    : m_FilePath(filepath), m_ID(s_NextID++)
{
    std::string source = ReadFile(filepath);
    auto sources = PreProcess(source);

    for (const auto& [stage, src] : sources)
        CollectDeclarations(src);
    LayoutUniforms();

    const std::string uboBlock    = BuildUBOBlock();
    const std::string samplerDecls = BuildSamplerDecls();

    for (const auto& [stage, src] : sources)
    {
        bool isVertex = (stage == kStageVertex);
        std::string body = RewriteStageBody(src, isVertex);
        std::string finalSrc = "#version 450 core\n" + uboBlock + samplerDecls + body;

        if (stage == kStageVertex)
        {
            auto spv = CompileStage(finalSrc, shaderc_vertex_shader, "vertex");
            m_VertModule = CreateModule(spv);
        }
        else
        {
            auto spv = CompileStage(finalSrc, shaderc_fragment_shader, "fragment");
            m_FragModule = CreateModule(spv);
        }
    }

    BuildShaderStages();
    CreatePipelineLayout();

    auto lastSlash = m_FilePath.find_last_of("/\\");
    auto lastDot   = m_FilePath.rfind('.');
    auto start     = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    auto count     = (lastDot == std::string::npos || lastDot < start)
                         ? std::string::npos : lastDot - start;
    m_Name = m_FilePath.substr(start, count);
}

VulkanShader::~VulkanShader()
{
    DestroyModules();
}

// ── Bind / Unbind ──────────────────────────────────────────────────

void VulkanShader::Bind() const
{
    if (auto* r = VulkanRendererAPI::Get())
        r->SetCurrentShader(const_cast<VulkanShader*>(this));
}

void VulkanShader::Unbind() const
{
    if (auto* r = VulkanRendererAPI::Get())
        r->SetCurrentShader(nullptr);
}

// ── File reading / preprocessing ───────────────────────────────────

std::string VulkanShader::ReadFile(std::string_view filepath) const
{
    std::ifstream in(std::string(filepath), std::ios::in | std::ios::binary);
    DMGE_CORE_ASSERT(in, "Could not open shader file: {0}", filepath);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

std::unordered_map<uint32_t, std::string>
VulkanShader::PreProcess(std::string_view source) const
{
    std::unordered_map<uint32_t, std::string> shaderSources;

    constexpr std::string_view typeToken = "#type";
    size_t pos = source.find(typeToken, 0);

    while (pos != std::string::npos)
    {
        size_t eol = source.find_first_of("\r\n", pos);
        DMGE_CORE_ASSERT(eol != std::string::npos, "Syntax error in shader source");

        size_t begin = pos + typeToken.size() + 1;
        std::string_view type = source.substr(begin, eol - begin);

        size_t nextLinePos = source.find_first_not_of("\r\n", eol);
        DMGE_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error in shader source");

        pos = source.find(typeToken, nextLinePos);

        uint32_t stage = ShaderTypeTokenToStage(type);
        DMGE_CORE_ASSERT(stage != ~0u, "Unknown shader type token: {0}", type);

        shaderSources[stage] = (pos == std::string::npos)
            ? std::string(source.substr(nextLinePos))
            : std::string(source.substr(nextLinePos, pos - nextLinePos));
    }
    return shaderSources;
}

// ── Declaration collection ─────────────────────────────────────────

void VulkanShader::CollectDeclarations(const std::string& source)
{
    // Matches: uniform <type> <name> [ [count] ] ;
    static const std::regex reUniform(
        R"(^\s*uniform\s+(\w+)\s+(\w+)\s*(?:\[\s*(\d+)\s*\])?\s*;)");
    // Matches: [layout(...)] in|out <type> <name> ;
    static const std::regex reInOut(
        R"(^\s*(layout\s*\([^)]*\)\s*)?(in|out)\s+(\w+)\s+(\w+)\s*(?:\[[^\]]*\])?\s*;)");

    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line))
    {
        std::string trimmed = StripComment(line);
        std::smatch m;
        if (std::regex_search(trimmed, m, reUniform))
        {
            std::string type = m[1].str();
            std::string name = m[2].str();
            uint32_t count = m[3].matched ? std::stoul(m[3].str()) : 1u;

            if (StartsWith(type, "sampler") || StartsWith(type, "texture"))
            {
                // Sampler uniform -> assign a descriptor binding.
                if (m_SamplerBindings.find(name) == m_SamplerBindings.end())
                {
                    uint32_t binding = static_cast<uint32_t>(m_Samplers.size()) + 1;
                    m_SamplerBindings[name] = binding;
                    m_Samplers.push_back({ name, type, binding });
                }
            }
            else
            {
                if (m_UniformIndex.find(name) == m_UniformIndex.end())
                {
                    m_UniformIndex[name] = m_UniformMembers.size();
                    UniformMember u;
                    u.name = name;
                    u.glslType = type;
                    u.count = count;
                    m_UniformMembers.push_back(std::move(u));
                }
            }
        }
        else
        {
            (void)reInOut; // locations are injected in RewriteStageBody
        }
    }
}

void VulkanShader::LayoutUniforms()
{
    uint32_t offset = 0;
    for (auto& u : m_UniformMembers)
    {
        Std140Layout l = TypeStd140Layout(u.glslType);
        if (u.count > 1)
        {
            // std140 arrays: every element is rounded up to a vec4 (16 bytes).
            uint32_t stride = RoundUp(l.size, 16);
            u.elementSize = stride;
            u.size = stride * u.count;
            offset = RoundUp(offset, 16);
        }
        else
        {
            u.elementSize = l.size;
            u.size = l.size;
            offset = RoundUp(offset, l.align);
        }
        u.offset = offset;
        offset += u.size;
    }
    m_UniformBlockSize = RoundUp(offset, 16);
    m_UniformData.assign(m_UniformBlockSize, 0);
}

std::string VulkanShader::BuildUBOBlock() const
{
    if (m_UniformMembers.empty())
        return {};

    std::ostringstream out;
    out << "layout(std140, set = 0, binding = 0) uniform _DMGE_Uniforms\n{\n";
    for (const auto& u : m_UniformMembers)
    {
        out << "    " << u.glslType << " " << u.name;
        if (u.count > 1)
            out << "[" << u.count << "]";
        out << ";\n";
    }
    out << "};\n\n";
    return out.str();
}

std::string VulkanShader::BuildSamplerDecls() const
{
    if (m_Samplers.empty())
        return {};

    std::ostringstream out;
    for (const auto& s : m_Samplers)
        out << "layout(set = 0, binding = " << s.binding
            << ") uniform " << s.glslType << " " << s.name << ";\n";
    out << "\n";
    return out.str();
}

// Remove standalone uniform decls + version lines, inject locations.
std::string VulkanShader::RewriteStageBody(const std::string& source, bool isVertex) const
{
    static const std::regex reUniform(
        R"(^\s*uniform\s+\w+\s+\w+\s*(?:\[\s*\d+\s*\])?\s*;)");
    static const std::regex reVersion(R"(^\s*#version\b.*)");
    // [layout(...)] (in|out) <type> <name> [ [..] ] ;
    static const std::regex reInOut(
        R"(^\s*(layout\s*\(([^)]*)\)\s*)?(in|out)\s+(\w+)\s+(\w+)\s*(?:\[[^\]]*\])?\s*;)");

    std::ostringstream out;
    std::istringstream stream(source);
    std::string line;

    uint32_t inLoc  = 0;
    uint32_t outLoc = 0;

    while (std::getline(stream, line))
    {
        std::string trimmed = StripComment(line);

        if (std::regex_search(trimmed, reVersion))
            continue; // we inject our own #version
        if (std::regex_search(trimmed, reUniform))
            continue; // moved into the synthesized UBO / sampler decls

        std::smatch m;
        if (std::regex_search(trimmed, m, reInOut))
        {
            std::string layoutPart = m[2].str(); // inside layout(...)
            std::string dir        = m[3].str();
            // If a location is already specified, keep the line as-is.
            if (layoutPart.find("location") == std::string::npos)
            {
                uint32_t loc = (dir == "in") ? inLoc++ : outLoc++;
                // Re-emit with an explicit location qualifier.
                std::string rest = trimmed.substr(trimmed.find(dir));
                out << "layout(location = " << loc << ") " << rest << "\n";
                continue;
            }
        }

        out << line << "\n";
    }
    return out.str();
}

// ── SPIR-V compilation ─────────────────────────────────────────────

std::vector<uint32_t>
VulkanShader::CompileStage(const std::string& source,
                            shaderc_shader_kind kind,
                            const char* stageName) const
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc::SpvCompilationResult result =
        compiler.CompileGlslToSpv(source, kind, stageName, options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        DMGE_LOG_ERROR("Vulkan {0} shader compilation failed:\n{1}",
                       stageName, result.GetErrorMessage());
        DMGE_CORE_ASSERT(false, "Vulkan {0} shader compilation failed!", stageName);
        return {};
    }
    return std::vector<uint32_t>(result.cbegin(), result.cend());
}

VkShaderModule VulkanShader::CreateModule(const std::vector<uint32_t>& code) const
{
    if (code.empty())
        return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode     = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(VulkanDevice::Get().Device, &info, nullptr, &module));
    return module;
}

void VulkanShader::BuildShaderStages()
{
    m_Stages.clear();
    if (m_VertModule != VK_NULL_HANDLE)
    {
        VkPipelineShaderStageCreateInfo vert{};
        vert.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert.stage   = VK_SHADER_STAGE_VERTEX_BIT;
        vert.module   = m_VertModule;
        vert.pName    = "main";
        m_Stages.push_back(vert);
    }
    if (m_FragModule != VK_NULL_HANDLE)
    {
        VkPipelineShaderStageCreateInfo frag{};
        frag.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag.stage   = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag.module   = m_FragModule;
        frag.pName    = "main";
        m_Stages.push_back(frag);
    }
}

void VulkanShader::CreatePipelineLayout()
{
    auto& dev = VulkanDevice::Get();

    // Descriptor set layout: binding 0 = UBO (dynamic), 1..N = samplers.
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    if (m_UniformBlockSize > 0)
    {
        VkDescriptorSetLayoutBinding ubo{};
        ubo.binding         = 0;
        ubo.descriptorCount  = 1;
        ubo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        ubo.stageFlags       = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(ubo);
    }
    for (const auto& s : m_Samplers)
    {
        VkDescriptorSetLayoutBinding samp{};
        samp.binding         = s.binding;
        samp.descriptorCount  = 1;
        samp.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samp.stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(samp);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings     = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(dev.Device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

    VkPipelineLayoutCreateInfo pipeInfo{};
    pipeInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeInfo.setLayoutCount = 1;
    pipeInfo.pSetLayouts     = &m_DescriptorSetLayout;
    VK_CHECK(vkCreatePipelineLayout(dev.Device, &pipeInfo, nullptr, &m_PipelineLayout));
}

void VulkanShader::DestroyModules()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(dev.Device);
    if (m_PipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(dev.Device, m_PipelineLayout, nullptr);
    if (m_DescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(dev.Device, m_DescriptorSetLayout, nullptr);
    if (m_VertModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(dev.Device, m_VertModule, nullptr);
    if (m_FragModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(dev.Device, m_FragModule, nullptr);
    m_PipelineLayout = m_DescriptorSetLayout = VK_NULL_HANDLE;
    m_VertModule = m_FragModule = VK_NULL_HANDLE;
}

// ── Uniform uploads ─────────────────────────────────────────────────

void VulkanShader::WriteUniform(std::string_view name, const void* data, uint32_t size)
{
    auto it = m_UniformIndex.find(std::string(name));
    if (it == m_UniformIndex.end())
        return; // unknown uniform (e.g. sampler-array slot setter) - ignore

    const auto& u = m_UniformMembers[it->second];
    uint32_t toWrite = (size < u.size) ? size : u.size;
    if (u.offset + toWrite > m_UniformData.size())
        return;
    std::memcpy(m_UniformData.data() + u.offset, data, toWrite);
}

void VulkanShader::WriteUniformArray(std::string_view name, const int* values, uint32_t count)
{
    auto it = m_UniformIndex.find(std::string(name));
    if (it == m_UniformIndex.end())
        return;

    const auto& u = m_UniformMembers[it->second];
    uint32_t n = std::min(count, u.count);
    for (uint32_t i = 0; i < n; ++i)
    {
        uint32_t dst = u.offset + i * u.elementSize;
        if (dst + sizeof(int) > m_UniformData.size())
            break;
        std::memcpy(m_UniformData.data() + dst, &values[i], sizeof(int));
    }
}

void VulkanShader::SetInt(std::string_view name, int value)
{
    WriteUniform(name, &value, sizeof(int));
}

void VulkanShader::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    WriteUniformArray(name, values, count);
}

void VulkanShader::SetFloat(std::string_view name, float value)
{
    WriteUniform(name, &value, sizeof(float));
}

void VulkanShader::SetFloat2(std::string_view name, const glm::vec2& value)
{
    WriteUniform(name, &value, sizeof(glm::vec2));
}

void VulkanShader::SetFloat3(std::string_view name, const glm::vec3& value)
{
    WriteUniform(name, &value, sizeof(glm::vec3));
}

void VulkanShader::SetFloat4(std::string_view name, const glm::vec4& value)
{
    WriteUniform(name, &value, sizeof(glm::vec4));
}

void VulkanShader::SetMat4(std::string_view name, const glm::mat4& value)
{
    WriteUniform(name, &value, sizeof(glm::mat4));
}

} // namespace DMGameEngine
