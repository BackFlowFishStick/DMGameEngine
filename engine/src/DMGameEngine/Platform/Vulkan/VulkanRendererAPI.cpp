/*
 * DMGameEngine - Vulkan Renderer API Implementation
 */

#include "DMGameEngine/Platform/Vulkan/VulkanRendererAPI.h"
#include "DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.h"
#include "DMGameEngine/Platform/Vulkan/VulkanDevice.h"
#include "DMGameEngine/Platform/Vulkan/VulkanShader.h"
#include "DMGameEngine/Platform/Vulkan/VulkanVertexArray.h"
#include "DMGameEngine/Platform/Vulkan/VulkanTexture.h"
#include "DMGameEngine/Platform/Vulkan/VulkanFrameBuffer.h"

#include "DMGameEngine/Renderer/VertexArray.h"

#include "DMGameEngine/Core/Log.h"

#include <cstring>
#include <cstdio>
#include <array>
#include <functional>
#include <vector>

namespace DMGameEngine {

VulkanRendererAPI* VulkanRendererAPI::s_Instance = nullptr;

namespace {

// ── Enum converters (engine -> Vulkan) ───────────────────────────

VkBlendFactor BlendFactorToVk(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::ConstantAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case BlendFactor::OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendFactor!");
    return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp BlendEquationToVk(BlendEquation e)
{
    switch (e)
    {
        case BlendEquation::Add:             return VK_BLEND_OP_ADD;
        case BlendEquation::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendEquation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendEquation::Min:             return VK_BLEND_OP_MIN;
        case BlendEquation::Max:             return VK_BLEND_OP_MAX;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendEquation!");
    return VK_BLEND_OP_ADD;
}

VkCompareOp DepthFuncToVk(DepthFunc f)
{
    switch (f)
    {
        case DepthFunc::Never:         return VK_COMPARE_OP_NEVER;
        case DepthFunc::Less:          return VK_COMPARE_OP_LESS;
        case DepthFunc::Equal:         return VK_COMPARE_OP_EQUAL;
        case DepthFunc::LessEqual:     return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthFunc::Greater:       return VK_COMPARE_OP_GREATER;
        case DepthFunc::NotEqual:      return VK_COMPARE_OP_NOT_EQUAL;
        case DepthFunc::GreaterEqual:  return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthFunc::Always:        return VK_COMPARE_OP_ALWAYS;
    }
    DMGE_CORE_ASSERT(false, "Unknown DepthFunc!");
    return VK_COMPARE_OP_LESS;
}

VkCullModeFlags CullModeToVk(CullMode m)
{
    switch (m)
    {
        case CullMode::None:         return VK_CULL_MODE_NONE;
        case CullMode::Front:        return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:         return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    DMGE_CORE_ASSERT(false, "Unknown CullMode!");
    return VK_CULL_MODE_NONE;
}

VkDeviceSize AlignUp(VkDeviceSize v, VkDeviceSize a)
{
    return (v + a - 1) & ~(a - 1);
}

// ── Descriptor-set cache (P0-1) ──────────────────────────────────────
//
// Key = (shaderID, hash of the bound texture handles). All descriptor
// content is covered by this key:
//   - binding 0 (dynamic UBO): buffer handle + range are constant within
//     a frame (the per-frame scratch buffer); the per-draw variation is
//     the dynamic offset, applied at vkCmdBindDescriptorSets, not stored
//     in the set.
//   - bindings 1..N (combined image samplers): the exact sampler + view
//     handles that WriteDescriptorSet writes, with the global 1x1 dummy
//     substituted for unbound slots (deterministic).
//   - the descriptor set layout itself is owned by the VulkanShader
//     instance; shaderID identifies that instance uniquely (monotonic,
//     never reused), so a key can never alias another shader's layout.
//
// Lifetime: one map per frame-in-flight slot, cleared together with the
// descriptor pool in ResetFrame (pool reset invalidates every set allocated
// from it). Storage is file-static (R1: no STL members on an exported
// class; same pattern as VulkanDevice's deletion buckets).
struct DescKey
{
    uint64_t shaderID = 0;
    uint64_t texHash  = 0;
    bool operator==(const DescKey& o) const { return shaderID == o.shaderID && texHash == o.texHash; }
};
struct DescKeyHash
{
    size_t operator()(const DescKey& k) const noexcept
    {
        size_t h = std::hash<uint64_t>{}(k.shaderID);
        h ^= std::hash<uint64_t>{}(k.texHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Must match VulkanRendererAPI::kMaxFramesInFlight.
constexpr uint32_t kDescCacheFrameSlots = 2;

std::unordered_map<DescKey, VkDescriptorSet, DescKeyHash>&
DescriptorCache(uint32_t frameIndex)
{
    static std::unordered_map<DescKey, VkDescriptorSet, DescKeyHash>
        s_Cache[kDescCacheFrameSlots];
    return s_Cache[frameIndex % kDescCacheFrameSlots];
}

void PurgeShaderFromDescriptorCache(uint64_t shaderID)
{
    for (uint32_t i = 0; i < kDescCacheFrameSlots; ++i)
    {
        auto& cache = DescriptorCache(i);
        for (auto it = cache.begin(); it != cache.end();)
        {
            if (it->first.shaderID == shaderID)
                it = cache.erase(it);
            else
                ++it;
        }
    }
}

// ── Pipeline cache serialization (P1: cold-start pipeline build skip) ──
//
// The VkPipelineCache is dumped to disk when the renderer API shuts down and
// loaded back at Init, so the second launch reuses the driver's pipeline
// binaries instead of recompiling every pipeline from SPIR-V.
//
// File: "vulkan_pipeline_cache.bin" in the process CWD (next to where the
// editor/game exe runs). Format:
//   [PipelineCacheHeader (44 bytes, little-endian host layout)] [driver blob]
// The header pins the file to the exact device identity (vendor/device IDs,
// driver version, API version, pipelineCacheUUID) - any mismatch, corruption,
// truncation or version bump discards the file and starts with an empty
// cache (safe fallback; the file is simply rewritten on the next clean
// shutdown). Driver pipeline-cache blobs are opaque and keyed by the
// pipelineCacheUUID, which is why the UUID is the primary validator.
constexpr uint32_t kPipelineCacheMagic   = 0x43504D44; // "DMPC" (LE)
constexpr uint32_t kPipelineCacheVersion = 1;
constexpr size_t   kPipelineCacheMaxSize = 64ull * 1024 * 1024; // sanity cap

struct PipelineCacheHeader
{
    uint32_t Magic;
    uint32_t Version;
    uint32_t VendorID;
    uint32_t DeviceID;
    uint32_t DriverVersion;
    uint32_t ApiVersion;
    uint8_t  CacheUUID[VK_UUID_SIZE];
    uint32_t DataSize; // bytes of driver blob following the header
};
static_assert(sizeof(PipelineCacheHeader) == 24 + VK_UUID_SIZE + 4,
              "PipelineCacheHeader layout must be padding-free");

const char* PipelineCacheFilePath() { return "vulkan_pipeline_cache.bin"; }

FILE* OpenFileOrNull(const char* path, const char* mode)
{
    FILE* f = nullptr;
#if defined(_MSC_VER)
    fopen_s(&f, path, mode);
#else
    f = std::fopen(path, mode);
#endif
    return f;
}

// Returns the driver blob if the on-disk cache is valid for THIS device,
// empty otherwise (any failure logs a warning and degrades to empty).
std::vector<uint8_t> LoadPipelineCacheData(const VulkanDevice& dev)
{
    const char* path = PipelineCacheFilePath();
    FILE* f = OpenFileOrNull(path, "rb");
    if (!f)
        return {}; // first run / no file: normal, not a warning

    auto discard = [f](const char* why) {
        DMGE_LOG_WARN("Vulkan: ignoring pipeline cache file ({0}).", why);
        std::fclose(f);
        return std::vector<uint8_t>{};
    };

    PipelineCacheHeader h{};
    if (std::fread(&h, 1, sizeof(h), f) != sizeof(h))
        return discard("truncated header");

    const auto& p = dev.Properties;
    if (h.Magic != kPipelineCacheMagic)
        return discard("bad magic");
    if (h.Version != kPipelineCacheVersion)
        return discard("version mismatch");
    if (h.VendorID != p.vendorID || h.DeviceID != p.deviceID ||
        h.DriverVersion != p.driverVersion || h.ApiVersion != p.apiVersion)
        return discard("device/driver identity mismatch");
    if (std::memcmp(h.CacheUUID, p.pipelineCacheUUID, VK_UUID_SIZE) != 0)
        return discard("pipelineCacheUUID mismatch");
    if (h.DataSize == 0 || h.DataSize > kPipelineCacheMaxSize)
        return discard("unreasonable payload size");

    std::vector<uint8_t> data(h.DataSize);
    if (std::fread(data.data(), 1, data.size(), f) != data.size())
        return discard("truncated payload");
    if (std::fgetc(f) != EOF)
        return discard("trailing garbage");

    std::fclose(f);
    DMGE_LOG_INFO("Vulkan: pipeline cache loaded ({0} bytes, {1}).",
                  data.size(), path);
    return data;
}

// Dumps the cache (header + driver blob) to a temp file, then atomically
// replaces the real one. Failure to save is a warning only - the cache is a
// pure optimization and the next run simply rebuilds pipelines.
void SavePipelineCacheData(const VulkanDevice& dev, VkPipelineCache cache)
{
    size_t size = 0;
    if (vkGetPipelineCacheData(dev.Device, cache, &size, nullptr) != VK_SUCCESS || size == 0)
    {
        DMGE_LOG_WARN("Vulkan: vkGetPipelineCacheData returned no data; pipeline cache not saved.");
        return;
    }
    if (size > kPipelineCacheMaxSize)
    {
        DMGE_LOG_WARN("Vulkan: pipeline cache too large ({0} bytes); not saved.", size);
        return;
    }

    std::vector<uint8_t> data(size);
    if (vkGetPipelineCacheData(dev.Device, cache, &size, data.data()) != VK_SUCCESS)
    {
        DMGE_LOG_WARN("Vulkan: vkGetPipelineCacheData failed on second query; pipeline cache not saved.");
        return;
    }

    const auto& p = dev.Properties;
    PipelineCacheHeader h{};
    h.Magic         = kPipelineCacheMagic;
    h.Version       = kPipelineCacheVersion;
    h.VendorID      = p.vendorID;
    h.DeviceID      = p.deviceID;
    h.DriverVersion = p.driverVersion;
    h.ApiVersion    = p.apiVersion;
    std::memcpy(h.CacheUUID, p.pipelineCacheUUID, VK_UUID_SIZE);
    h.DataSize = static_cast<uint32_t>(data.size());

    const char* path = PipelineCacheFilePath();
    const char* tmpPath = "vulkan_pipeline_cache.bin.tmp";
    FILE* f = OpenFileOrNull(tmpPath, "wb");
    if (!f)
    {
        DMGE_LOG_WARN("Vulkan: cannot open '{0}' for writing; pipeline cache not saved.", tmpPath);
        return;
    }
    const bool ok = std::fwrite(&h, 1, sizeof(h), f) == sizeof(h) &&
                    std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);

    if (!ok || std::remove(path) != 0 || std::rename(tmpPath, path) != 0)
    {
        std::remove(tmpPath);
        DMGE_LOG_WARN("Vulkan: failed to replace '{0}'; pipeline cache not saved.", path);
        return;
    }
    DMGE_LOG_INFO("Vulkan: pipeline cache saved ({0} bytes -> {1}).", data.size(), path);
}

// ── Per-frame descriptor pools (P2: exhaustion warning + auto-grow) ──
//
// Storage is file-static (R1: no STL members on the exported class; same
// pattern as the descriptor cache above). Each frame slot owns a LIST of
// pools: when every pool is exhausted (vkAllocateDescriptorSets returns
// OUT_OF_POOL_MEMORY / FRAGMENTED), a new pool with doubled capacity is
// appended after a warning log, so a frame that genuinely needs more
// descriptor sets keeps working instead of silently dropping draws.
//
// Interaction with the P0-1 descriptor cache: growing KEEPS the old pools
// alive, so previously allocated sets (cached or not) stay valid - growth
// needs NO cache invalidation. Invalidation happens exactly once per frame
// in ResetFrame, where every pool is reset (invalidating all sets) and the
// cache is cleared - the unchanged P0-1 rule.
//
// Retained capacity is bounded: ResetFrame also destroys all but the largest
// pool (their sets were just freed and nothing references them), so
// steady-state memory tracks the peak frame instead of creeping upward.
constexpr uint32_t kPoolFrameSlots   = 2; // must match VulkanRendererAPI::kMaxFramesInFlight
constexpr uint32_t kBasePoolMaxSets  = 8192;
constexpr uint32_t kBasePoolSamplers = 32768;

struct DescriptorPoolSlot
{
    std::vector<VkDescriptorPool> Pools;
    std::vector<uint32_t>         PoolMaxSets;   // parallel to Pools
    std::vector<uint32_t>         PoolSamplers;  // parallel to Pools
    uint32_t NextMaxSets  = kBasePoolMaxSets;    // capacity of the next pool created
    uint32_t NextSamplers = kBasePoolSamplers;
    uint32_t SetsAllocatedThisFrame = 0;         // observability for the warning
};

DescriptorPoolSlot& PoolSlot(uint32_t frameIndex)
{
    static DescriptorPoolSlot s_Slots[kPoolFrameSlots];
    return s_Slots[frameIndex % kPoolFrameSlots];
}

void ResetPoolSlotBookkeeping(DescriptorPoolSlot& slot)
{
    slot.Pools.clear();
    slot.PoolMaxSets.clear();
    slot.PoolSamplers.clear();
    slot.NextMaxSets  = kBasePoolMaxSets;
    slot.NextSamplers = kBasePoolSamplers;
    slot.SetsAllocatedThisFrame = 0;
}

void DestroyDescriptorPools(VkDevice device, uint32_t frameIndex)
{
    auto& slot = PoolSlot(frameIndex);
    for (VkDescriptorPool pool : slot.Pools)
        vkDestroyDescriptorPool(device, pool, nullptr);
    ResetPoolSlotBookkeeping(slot);
}

void CreateDescriptorPoolInto(DescriptorPoolSlot& slot, VkDevice device)
{
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[0].descriptorCount = slot.NextMaxSets; // one dynamic UBO per set
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = slot.NextSamplers;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = slot.NextMaxSets;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
    slot.Pools.push_back(pool);
    slot.PoolMaxSets.push_back(slot.NextMaxSets);
    slot.PoolSamplers.push_back(slot.NextSamplers);

    // The next pool (if this one ever fills mid-frame) is twice as large.
    slot.NextMaxSets  *= 2;
    slot.NextSamplers *= 2;
}

VkDescriptorSet TryAllocateDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                         VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &info, &set) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return set;
}

} // anonymous namespace

// ── Lifetime ──────────────────────────────────────────────────────

VulkanRendererAPI::~VulkanRendererAPI()
{
    auto& dev = VulkanDevice::Get();
    vkDeviceWaitIdle(dev.Device);

    for (auto& [key, pipeline] : m_Pipelines)
        vkDestroyPipeline(dev.Device, pipeline, nullptr);
    m_Pipelines.clear();

    if (m_PipelineCache != VK_NULL_HANDLE)
    {
        // Persist the accumulated pipeline binaries before the cache object
        // dies (P1). The device is already idle (vkDeviceWaitIdle above).
        SavePipelineCacheData(dev, m_PipelineCache);
        vkDestroyPipelineCache(dev.Device, m_PipelineCache, nullptr);
    }

    DestroyDummyResources();
    DestroyFrameResources();

    if (s_Instance == this)
        s_Instance = nullptr;
}

void VulkanRendererAPI::Init(const RendererAPIInitConfig& config)
{
    s_Instance = this;

    // Apply the requested initial pipeline state through the virtual
    // setters (same as the base class, kept here for clarity / override).
    RendererAPI::Init(config);

    auto& dev = VulkanDevice::Get();

    // P1: seed the cache from disk (previous runs' pipeline binaries). If the
    // driver rejects the blob (corrupt / identity mismatch our header check
    // could not catch), fall back to an empty cache and continue.
    std::vector<uint8_t> initialCacheData = LoadPipelineCacheData(dev);

    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (!initialCacheData.empty())
    {
        cacheInfo.initialDataSize = initialCacheData.size();
        cacheInfo.pInitialData    = initialCacheData.data();
    }

    VkResult cacheResult = vkCreatePipelineCache(dev.Device, &cacheInfo, nullptr, &m_PipelineCache);
    if (cacheResult != VK_SUCCESS && !initialCacheData.empty())
    {
        DMGE_LOG_WARN("Vulkan: driver rejected the on-disk pipeline cache data "
                      "(VkResult {0}); starting with an empty cache.",
                      static_cast<int>(cacheResult));
        cacheInfo.initialDataSize = 0;
        cacheInfo.pInitialData    = nullptr;
        cacheResult = vkCreatePipelineCache(dev.Device, &cacheInfo, nullptr, &m_PipelineCache);
    }
    VK_CHECK(cacheResult);

    // Defensive pool-slot reset BEFORE creating frame resources (a previous
    // renderer-API instance's dtor already destroyed its pools; this only
    // restores base sizes/counters in case anything was left behind).
    for (uint32_t i = 0; i < kPoolFrameSlots; ++i)
        ResetPoolSlotBookkeeping(PoolSlot(i));

    CreateFrameResources();
    CreateDummyResources();

    // A previous renderer-API instance (runtime SetAPI switch) may have left
    // entries whose descriptor pools no longer exist; start clean.
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        DescriptorCache(i).clear();

    DMGE_LOG_INFO("Vulkan renderer API initialized.");
}

void VulkanRendererAPI::OnSwapchainRecreate()
{
    auto& dev = VulkanDevice::Get();

    // Destroy all cached pipelines. The next Clear()/BeginRenderPass will
    // refresh m_ActiveColorFormats/m_ActiveDepthFormat from the new swapchain
    // (or offscreen target), so pipelines are recreated lazily with the
    // correct format on their next draw. The VkPipelineCache object is kept.
    for (auto& [key, pipeline] : m_Pipelines)
    {
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(dev.Device, pipeline, nullptr);
    }
    m_Pipelines.clear();
}

// ── Frame / clear / viewport ──────────────────────────────────────

void VulkanRendererAPI::Clear()
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
    {
        ctx.BeginFrame(m_ClearColor);
        ResetFrame(ctx.GetCurrentFrame());
    }
    // The swapchain pass opened by BeginFrame is the active render target;
    // record its formats as the baseline for PipelineKey (overridden by an
    // offscreen BeginRenderPass).
    m_ActiveColorAttachmentCount = 1;
    m_ActiveColorFormats[0] = ctx.GetSwapchain().GetImageFormat();
    m_ActiveDepthFormat = ctx.GetSwapchain().GetDepthFormat();
    // If a frame is already started, the framebuffer was cleared at
    // BeginFrame (loadOp = CLEAR); nothing more to do.
}

void VulkanRendererAPI::BeginRenderPass(FrameBuffer* target)
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return; // no frame yet: nothing to bind (defensive)

    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();

    // Default / swapchain target: the swapchain pass is already open from
    // BeginFrame (Clear). Just record its formats as the active pipeline
    // formats and leave the pass open.
    const bool offscreen = (target != nullptr) && !target->GetSpecification().SwapChainTarget;
    if (!offscreen)
    {
        auto& swap = ctx.GetSwapchain();
        m_ActiveColorAttachmentCount = 1;
        m_ActiveColorFormats[0] = swap.GetImageFormat();
        m_ActiveDepthFormat = swap.GetDepthFormat();
        m_ActiveFrameBuffer = nullptr;
        return;
    }

    auto* fbo = static_cast<VulkanFrameBuffer*>(target);
    DMGE_CORE_ASSERT(fbo->GetColorAttachmentCount() > 0,
                     "Vulkan: offscreen FrameBuffer has no color attachment (depth-only RT not supported yet).");
    m_ActiveFrameBuffer = fbo;

    // End the swapchain dynamic-rendering pass BeginFrame opened so we can
    // begin one targeting the FBO (vkCmdBeginRendering cannot nest).
    vkCmdEndRendering(cmd);

    // Transition FBO color/depth images UNDEFINED -> attachment layout. UNDEFINED
    // discards prior contents (we clear), so this is valid regardless of layout.
    for (uint32_t i = 0; i < fbo->GetColorAttachmentCount(); ++i)
    {
        VulkanGraphicsContext::TransitionImageLayout(
            cmd, fbo->GetColorImage(i), fbo->GetColorFormat(i),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    if (fbo->HasDepth())
    {
        VulkanGraphicsContext::TransitionImageLayout(
            cmd, fbo->GetDepthImage(), fbo->GetDepthFormat(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    VkClearValue colorClear{};
    colorClear.color.float32[0] = m_ClearColor.r;
    colorClear.color.float32[1] = m_ClearColor.g;
    colorClear.color.float32[2] = m_ClearColor.b;
    colorClear.color.float32[3] = m_ClearColor.a;

    // MRT (3e): one attachment info per color attachment of the FBO, all
    // cleared to the app clear color and stored.
    const uint32_t colorCount = static_cast<uint32_t>(fbo->GetColorAttachmentCount());
    DMGE_CORE_ASSERT(colorCount <= kMaxColorAttachments,
                     "Vulkan: offscreen FrameBuffer exceeds kMaxColorAttachments!");
    std::array<VkRenderingAttachmentInfo, kMaxColorAttachments> colorAttachments{};
    for (uint32_t i = 0; i < colorCount && i < kMaxColorAttachments; ++i)
    {
        VkRenderingAttachmentInfo& att = colorAttachments[i];
        att.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        att.imageView    = fbo->GetColorImageView(i);
        att.imageLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;
        att.clearValue   = colorClear;
    }

    VkRenderingAttachmentInfo depthAttachment{};
    if (fbo->HasDepth())
    {
        VkClearValue depthClear{};
        depthClear.depthStencil.depth  = 1.0f;
        depthClear.depthStencil.stencil = 0;
        depthAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView    = fbo->GetDepthImageView();
        depthAttachment.imageLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue  = depthClear;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset   = { 0, 0 };
    renderingInfo.renderArea.extent   = { fbo->GetWidth(), fbo->GetHeight() };
    renderingInfo.layerCount          = 1;
    renderingInfo.colorAttachmentCount = colorCount;
    renderingInfo.pColorAttachments   = colorAttachments.data();
    renderingInfo.pDepthAttachment    = fbo->HasDepth() ? &depthAttachment : nullptr;
    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(fbo->GetWidth());
    viewport.height   = static_cast<float>(fbo->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { fbo->GetWidth(), fbo->GetHeight() };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_ActiveColorAttachmentCount = colorCount;
    for (uint32_t i = 0; i < colorCount && i < kMaxColorAttachments; ++i)
        m_ActiveColorFormats[i] = fbo->GetColorFormat(i);
    m_ActiveDepthFormat = fbo->GetDepthFormat();
    m_InOffscreenPass   = true;
}

void VulkanRendererAPI::EndRenderPass()
{
    if (!m_InOffscreenPass)
        return; // swapchain pass: stays open until SwapBuffers (EndFrame).

    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return;
    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();
    auto* fbo = m_ActiveFrameBuffer;

    vkCmdEndRendering(cmd); // end the offscreen pass

    // Transition the FBO color/depth images -> SHADER_READ_ONLY so the rendered
    // result can be sampled by a later pass (post-process, shadow maps, ...).
    if (fbo)
    {
        for (uint32_t i = 0; i < fbo->GetColorAttachmentCount(); ++i)
        {
            VulkanGraphicsContext::TransitionImageLayout(
                cmd, fbo->GetColorImage(i), fbo->GetColorFormat(i),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
        if (fbo->HasDepth())
        {
            VulkanGraphicsContext::TransitionImageLayout(
                cmd, fbo->GetDepthImage(), fbo->GetDepthFormat(),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
    }

    // Restart the swapchain pass with loadOp = LOAD so earlier scene output is
    // preserved and subsequent scene layers + ImGui keep drawing to the swapchain.
    auto& swap = ctx.GetSwapchain();
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView    = swap.GetImageView(ctx.GetCurrentImageIndex());
    colorAttachment.imageLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView    = swap.GetDepthView();
    depthAttachment.imageLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp      = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset   = { 0, 0 };
    renderingInfo.renderArea.extent   = swap.GetExtent();
    renderingInfo.layerCount          = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments   = &colorAttachment;
    renderingInfo.pDepthAttachment    = &depthAttachment;
    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swap.GetExtent().width);
    viewport.height   = static_cast<float>(swap.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swap.GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_ActiveColorAttachmentCount = 1;
    m_ActiveColorFormats[0] = swap.GetImageFormat();
    m_ActiveDepthFormat = swap.GetDepthFormat();
    m_InOffscreenPass   = false;
    m_ActiveFrameBuffer = nullptr;
}
void VulkanRendererAPI::SetViewport(int x, int y, int width, int height)
{
    m_ViewportX = x; m_ViewportY = y;
    m_ViewportW = width; m_ViewportH = height;

    auto& ctx = VulkanGraphicsContext::Get();
    if (ctx.IsFrameStarted() && width > 0 && height > 0)
    {
        VkViewport viewport{};
        viewport.x        = static_cast<float>(x);
        viewport.y        = static_cast<float>(y);
        viewport.width     = static_cast<float>(width);
        viewport.height    = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(ctx.GetCurrentCommandBuffer(), 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { x, y };
        scissor.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        vkCmdSetScissor(ctx.GetCurrentCommandBuffer(), 0, 1, &scissor);
    }
}

// ── Draw ──────────────────────────────────────────────────────────

void VulkanRendererAPI::DrawIndexed(const VertexArray& vertexArray)
{
    DrawIndexedCommon(vertexArray, 1, 0);
}

void VulkanRendererAPI::DrawIndexedInstanced(const VertexArray& vertexArray,
                                             uint32_t instanceCount,
                                             uint32_t baseInstance)
{
    DrawIndexedCommon(vertexArray, instanceCount, baseInstance);
}

void VulkanRendererAPI::DrawIndexedCommon(const VertexArray& vertexArray,
                                          uint32_t instanceCount,
                                          uint32_t firstInstance)
{
    auto& ctx = VulkanGraphicsContext::Get();
    if (!ctx.IsFrameStarted())
        return;

    DMGE_CORE_ASSERT(m_CurrentShader, "Vulkan: DrawIndexed called with no shader bound!");
    auto& shader = *m_CurrentShader;

    const auto& va = static_cast<const VulkanVertexArray&>(vertexArray);
    VkCommandBuffer cmd = ctx.GetCurrentCommandBuffer();
    uint32_t frame = ctx.GetCurrentFrame();

    VkDeviceSize dynamicOffset = 0;
    bool hasUBO = (shader.GetUniformBlockSize() > 0);
    if (hasUBO)
        dynamicOffset = CommitUniforms(shader, frame);

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (shader.GetDescriptorSetLayout() != VK_NULL_HANDLE)
    {
        // P0-1: reuse the set from a previous draw with identical bindings
        // (same shader + same texture handles) instead of allocating and
        // rewriting one per draw. The UBO is dynamic: only the offset
        // passed to vkCmdBindDescriptorSets changes per draw.
        DescKey key{ shader.GetID(), HashBoundTextures(shader) };
        auto& cache = DescriptorCache(frame);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            set = it->second;
        }
        else
        {
            set = AllocateDescriptorSet(shader.GetDescriptorSetLayout(), frame);
            if (set != VK_NULL_HANDLE)
            {
                WriteDescriptorSet(set, shader, frame);
                cache.emplace(key, set);
            }
        }
    }

    va.Bind();

    VkPipeline pipeline = GetOrCreatePipeline(shader, va);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (set != VK_NULL_HANDLE)
    {
        uint32_t dynCount = hasUBO ? 1 : 0;
        uint32_t dynOffset = hasUBO ? static_cast<uint32_t>(dynamicOffset) : 0;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shader.GetPipelineLayout(),
                                0, 1, &set, dynCount,
                                hasUBO ? &dynOffset : nullptr);
    }

    vkCmdDrawIndexed(cmd, va.GetIndexCount(), instanceCount, 0, 0, firstInstance);
}

// ── Pipeline state setters ────────────────────────────────────────

void VulkanRendererAPI::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    m_BlendEnabled = enable;
    m_SrcBlend = srcFactor;
    m_DstBlend = dstFactor;
}

void VulkanRendererAPI::SetBlendEquation(BlendEquation equation)
{
    m_BlendEquation = equation;
}

void VulkanRendererAPI::SetDepthTest(bool enable) { m_DepthTestEnabled = enable; }
void VulkanRendererAPI::SetDepthFunc(DepthFunc func) { m_DepthFunc = func; }
void VulkanRendererAPI::SetCullMode(CullMode mode) { m_CullMode = mode; }

void VulkanRendererAPI::SetBoundTexture(uint32_t slot, VulkanTexture* texture)
{
    if (slot < kMaxBoundTextures)
        m_BoundTextures[slot] = texture;
}

// ── Pipeline key ──────────────────────────────────────────────────

bool VulkanRendererAPI::PipelineKey::operator==(const PipelineKey& o) const
{
    if (shaderID         != o.shaderID ||
        vertexLayoutHash != o.vertexLayoutHash ||
        blendEnabled     != o.blendEnabled ||
        srcBlend          != o.srcBlend ||
        dstBlend          != o.dstBlend ||
        blendEq           != o.blendEq ||
        depthTestEnabled != o.depthTestEnabled ||
        depthFunc         != o.depthFunc ||
        cullMode          != o.cullMode ||
        colorAttachmentCount != o.colorAttachmentCount ||
        depthFormat      != o.depthFormat)
        return false;

    // Only the active attachment formats participate: a key's unused tail
    // entries may hold stale values from a previous pass.
    for (uint32_t i = 0; i < colorAttachmentCount && i < kMaxColorAttachments; ++i)
        if (colorFormats[i] != o.colorFormats[i])
            return false;
    return true;
}

size_t VulkanRendererAPI::PipelineKeyHash::operator()(const PipelineKey& k) const noexcept
{
    size_t h = std::hash<uint64_t>{}(k.shaderID);
    h ^= std::hash<uint64_t>{}(k.vertexLayoutHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.blendEnabled) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.srcBlend)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.dstBlend)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.blendEq)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.depthTestEnabled) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.depthFunc)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.cullMode)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(k.colorAttachmentCount) + 0x9e3779b9 + (h << 6) + (h >> 2);
    // Only the active attachment formats participate (mirrors operator==).
    for (uint32_t i = 0; i < k.colorAttachmentCount && i < kMaxColorAttachments; ++i)
        h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.colorFormats[i])) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.depthFormat)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

VkPipeline VulkanRendererAPI::GetOrCreatePipeline(VulkanShader& shader,
                                                    const VulkanVertexArray& va)
{
    PipelineKey key;
    key.shaderID          = shader.GetID();
    key.vertexLayoutHash  = va.GetLayoutHash();
    key.blendEnabled       = m_BlendEnabled;
    key.srcBlend           = m_SrcBlend;
    key.dstBlend           = m_DstBlend;
    key.blendEq            = m_BlendEquation;
    key.depthTestEnabled   = m_DepthTestEnabled;
    key.depthFunc          = m_DepthFunc;
    key.cullMode           = m_CullMode;
    key.colorAttachmentCount = m_ActiveColorAttachmentCount;
    for (uint32_t i = 0; i < m_ActiveColorAttachmentCount && i < kMaxColorAttachments; ++i)
        key.colorFormats[i] = m_ActiveColorFormats[i];
    key.depthFormat        = m_ActiveDepthFormat;

    auto it = m_Pipelines.find(key);
    if (it != m_Pipelines.end())
        return it->second;

    auto& dev   = VulkanDevice::Get();
    const uint32_t colorCount = m_ActiveColorAttachmentCount;
    VkFormat depthFormat  = m_ActiveDepthFormat;

    // ── Shader stages ────────────────────────────────────────────
    const auto& stages = shader.GetShaderStages();

    // ── Vertex input ─────────────────────────────────────────────
    const auto& bindings   = va.GetBindingDescriptions();
    const auto& attributes  = va.GetAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions       = bindings.data();
    vertexInput.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions     = attributes.data();

    // ── Input assembly ───────────────────────────────────────────
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Viewport / scissor (dynamic) ─────────────────────────────
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount   = 1;

    // ── Rasterizer ───────────────────────────────────────────────
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType        = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = CullModeToVk(m_CullMode);
    rasterizer.frontFace    = VK_FRONT_FACE_CLOCKWISE;  // CW: matches the projection Y-flip in Renderer::BeginScene
    rasterizer.lineWidth   = 1.0f;

    // ── Multisampling ────────────────────────────────────────────
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable   = VK_FALSE;

    // ── Depth / stencil ──────────────────────────────────────────
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable   = m_DepthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable  = m_DepthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp    = DepthFuncToVk(m_DepthFunc);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable    = VK_FALSE;

    // ── Color blend ───────────────────────────────────────────────
    // MRT (3e): the blend state is engine-global (mirrors the OpenGL
    // backend's uniform glBlendFunc), so the SAME state is replicated to
    // every color attachment; the attachment COUNT must match the active
    // pass (baked into the pipeline + PipelineKey).
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable         = m_BlendEnabled ? VK_TRUE : VK_FALSE;
    blendAttachment.srcColorBlendFactor = BlendFactorToVk(m_SrcBlend);
    blendAttachment.dstColorBlendFactor = BlendFactorToVk(m_DstBlend);
    blendAttachment.colorBlendOp         = BlendEquationToVk(m_BlendEquation);
    blendAttachment.srcAlphaBlendFactor = BlendFactorToVk(m_SrcBlend);
    blendAttachment.dstAlphaBlendFactor = BlendFactorToVk(m_DstBlend);
    blendAttachment.alphaBlendOp         = BlendEquationToVk(m_BlendEquation);
    blendAttachment.colorWriteMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::array<VkPipelineColorBlendAttachmentState, kMaxColorAttachments> blendAttachments{};
    blendAttachments.fill(blendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable   = VK_FALSE;
    colorBlend.attachmentCount = colorCount;
    colorBlend.pAttachments     = blendAttachments.data();

    // ── Dynamic state ────────────────────────────────────────────
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount  = 2;
    dynamicState.pDynamicStates      = dynamicStates;

    // ── Dynamic rendering info ───────────────────────────────────
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount     = colorCount;
    renderingInfo.pColorAttachmentFormats = m_ActiveColorFormats;
    renderingInfo.depthAttachmentFormat    = depthFormat;

    // ── Assemble ─────────────────────────────────────────────────
    VkGraphicsPipelineCreateInfo info{};
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext               = &renderingInfo;
    info.stageCount           = static_cast<uint32_t>(stages.size());
    info.pStages              = stages.data();
    info.pVertexInputState    = &vertexInput;
    info.pInputAssemblyState   = &inputAssembly;
    info.pViewportState        = &viewportState;
    info.pRasterizationState   = &rasterizer;
    info.pMultisampleState     = &multisampling;
    info.pDepthStencilState   = &depthStencil;
    info.pColorBlendState      = &colorBlend;
    info.pDynamicState         = &dynamicState;
    info.layout                = shader.GetPipelineLayout();
    info.renderPass            = VK_NULL_HANDLE; // dynamic rendering
    info.subpass               = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(dev.Device, m_PipelineCache, 1, &info, nullptr, &pipeline));

    m_Pipelines[key] = pipeline;
    return pipeline;
}

// ── Per-frame scratch resources ───────────────────────────────────

void VulkanRendererAPI::CreateFrameResources()
{
    auto& dev = VulkanDevice::Get();

    VkDeviceSize align = dev.MinUniformBufferOffsetAlignment;
    if (align == 0) align = 16;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        // Descriptor pool(s): one set per draw (UBO + samplers). Created at
        // base capacity; grows on exhaustion (see AllocateDescriptorSet).
        CreateDescriptorPoolInto(PoolSlot(i), dev.Device);

        // Host-visible uniform scratch buffer.
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = kUniformBufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocResult{};
        VK_CHECK(vmaCreateBuffer(dev.Allocator, &bufInfo, &allocInfo,
                                 &m_UniformBuffers[i], &m_UniformAllocs[i], &allocResult));
        m_UniformMapped[i] = allocResult.pMappedData;
        m_UniformOffset[i] = 0;
    }
}

void VulkanRendererAPI::DestroyFrameResources()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (m_UniformBuffers[i] != VK_NULL_HANDLE)
            vmaDestroyBuffer(dev.Allocator, m_UniformBuffers[i], m_UniformAllocs[i]);
        DestroyDescriptorPools(dev.Device, i);
        m_UniformBuffers[i] = VK_NULL_HANDLE;
        m_UniformMapped[i] = nullptr;
        m_UniformOffset[i] = 0;
    }
}

void VulkanRendererAPI::CreateDummyResources()
{
    auto& dev = VulkanDevice::Get();

    // 1x1 RGBA8 image for unbound sampler slots. No pixel data is uploaded:
    // an UNDEFINED -> SHADER_READ_ONLY_OPTIMAL transition is valid and leaves
    // contents undefined (the dummy only exists to satisfy the descriptor
    // binding; sampling it returns 0/black, acceptable for an unused slot).
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { 1, 1, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    VK_CHECK(vmaCreateImage(dev.Allocator, &imageInfo, &allocInfo,
                            &m_DummyImage, &m_DummyAlloc, nullptr));

    dev.ImmediateSubmit([this](VkCommandBuffer cmd) {
        VulkanGraphicsContext::TransitionImageLayout(cmd, m_DummyImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = m_DummyImage;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    VK_CHECK(vkCreateImageView(dev.Device, &viewInfo, nullptr, &m_DummyView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter     = VK_FILTER_NEAREST;
    samplerInfo.minFilter     = VK_FILTER_NEAREST;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;
    samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable   = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = 0.0f;
    VK_CHECK(vkCreateSampler(dev.Device, &samplerInfo, nullptr, &m_DummySampler));
}

void VulkanRendererAPI::DestroyDummyResources()
{
    auto& dev = VulkanDevice::Get();
    if (dev.Device == VK_NULL_HANDLE)
        return;

    if (m_DummySampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(dev.Device, m_DummySampler, nullptr);
        m_DummySampler = VK_NULL_HANDLE;
    }
    if (m_DummyView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(dev.Device, m_DummyView, nullptr);
        m_DummyView = VK_NULL_HANDLE;
    }
    if (m_DummyImage != VK_NULL_HANDLE)
    {
        vmaDestroyImage(dev.Allocator, m_DummyImage, m_DummyAlloc);
        m_DummyImage = VK_NULL_HANDLE;
        m_DummyAlloc = VK_NULL_HANDLE;
    }
}

void VulkanRendererAPI::ResetFrame(uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    auto& slot = PoolSlot(frameIndex);

    // Reset every pool in the slot (a frame may have grown to several).
    for (VkDescriptorPool pool : slot.Pools)
        vkResetDescriptorPool(dev.Device, pool, 0);

    // Shrink retained capacity back to the peak pool: all sets were just
    // freed by the reset and the P0-1 cache below is cleared, so nothing
    // references the smaller pools any more. Steady-state memory tracks the
    // peak frame instead of creeping upward across a long session.
    if (slot.Pools.size() > 1)
    {
        size_t keep = 0;
        for (size_t i = 1; i < slot.Pools.size(); ++i)
            if (slot.PoolMaxSets[i] > slot.PoolMaxSets[keep])
                keep = i;
        const uint32_t keptMaxSets  = slot.PoolMaxSets[keep];
        const uint32_t keptSamplers = slot.PoolSamplers[keep];
        const size_t poolCount = slot.Pools.size();
        for (size_t i = 0; i < poolCount; ++i)
        {
            if (i == keep) continue;
            vkDestroyDescriptorPool(dev.Device, slot.Pools[i], nullptr);
        }
        VkDescriptorPool keptPool = slot.Pools[keep];
        ResetPoolSlotBookkeeping(slot);
        slot.Pools.push_back(keptPool);
        slot.PoolMaxSets.push_back(keptMaxSets);
        slot.PoolSamplers.push_back(keptSamplers);
        // Next growth doubles from the retained peak, not from base.
        slot.NextMaxSets  = keptMaxSets * 2;
        slot.NextSamplers = keptSamplers * 2;
        DMGE_LOG_INFO("Vulkan: descriptor pools shrank from {0} to 1 (retained "
                      "peak capacity {1} sets / {2} sampler descriptors).",
                      poolCount, keptMaxSets, keptSamplers);
    }

    // The pool reset invalidates every descriptor set allocated from the
    // pools, so the frame's cache must drop all entries (P0-1).
    DescriptorCache(frameIndex).clear();
    slot.SetsAllocatedThisFrame = 0;
    m_UniformOffset[frameIndex] = 0;
}

void VulkanRendererAPI::OnShaderDestroyed(uint64_t shaderID)
{
    // Defensive invalidation (see declaration comment): shader IDs are never
    // reused and the per-frame ResetFrame clear already bounds entry
    // lifetime, but purge eagerly so the maps never hold dead-shader keys.
    // The sets themselves are pool-owned and die at the pool reset.
    PurgeShaderFromDescriptorCache(shaderID);
}

uint64_t VulkanRendererAPI::HashBoundTextures(const VulkanShader& shader) const
{
    uint64_t h = 1469598103934665603ull; // FNV-1a offset basis
    for (uint32_t i = 0; i < shader.GetSamplerCount(); ++i)
    {
        VulkanTexture* tex = (i < kMaxBoundTextures) ? m_BoundTextures[i] : nullptr;
        // Unbound slots write the global dummy - hash the exact handles the
        // descriptor will contain so equivalent bindings collide on purpose.
        // (uintptr_t via reinterpret_cast: non-dispatchable handles are
        // pointer-typed with this SDK's headers, integer-typed otherwise.)
        VkSampler   sampler = tex ? tex->GetVkSampler()   : m_DummySampler;
        VkImageView view    = tex ? tex->GetVkImageView() : m_DummyView;
        h ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sampler)) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(view))    + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
}

uint32_t VulkanRendererAPI::CurrentFrameIndex() const
{
    return VulkanGraphicsContext::Get().GetCurrentFrame();
}

VkDeviceSize VulkanRendererAPI::CommitUniforms(VulkanShader& shader, uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    VkDeviceSize blockSize = shader.GetUniformBlockSize();
    VkDeviceSize align     = dev.MinUniformBufferOffsetAlignment;
    if (align == 0) align = 16;

    VkDeviceSize alignedSize = AlignUp(blockSize, align);
    VkDeviceSize offset = AlignUp(m_UniformOffset[frameIndex], align);

    if (offset + alignedSize > kUniformBufferSize)
    {
        DMGE_CORE_ASSERT(false, "Vulkan: per-frame uniform scratch buffer exhausted!");
        return 0;
    }

    std::memcpy(static_cast<uint8_t*>(m_UniformMapped[frameIndex]) + offset,
                shader.GetUniformData(), static_cast<size_t>(blockSize));

    m_UniformOffset[frameIndex] = offset + alignedSize;
    return offset;
}

VkDescriptorSet VulkanRendererAPI::AllocateDescriptorSet(VkDescriptorSetLayout layout, uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    auto& slot = PoolSlot(frameIndex);
    if (slot.Pools.empty())
        CreateDescriptorPoolInto(slot, dev.Device); // defensive re-init

    // Newest pool first: earlier pools only still have space before they
    // filled up mid-frame (they refill at the next ResetFrame).
    for (size_t i = slot.Pools.size(); i > 0; --i)
    {
        VkDescriptorSet set = TryAllocateDescriptorSet(dev.Device, slot.Pools[i - 1], layout);
        if (set != VK_NULL_HANDLE)
        {
            ++slot.SetsAllocatedThisFrame;
            return set;
        }
    }

    // P2: exhaustion is now observable - warn with the full picture, grow
    // (old pools stay alive, so cached sets from the P0-1 cache remain
    // valid) and retry once.
    DMGE_LOG_WARN("Vulkan: descriptor pools exhausted on frame slot {0} "
                  "({1} sets allocated this frame across {2} pool(s), largest "
                  "capacity {3} sets); growing - new pool capacity {4} sets / "
                  "{5} sampler descriptors.",
                  frameIndex, slot.SetsAllocatedThisFrame, slot.Pools.size(),
                  slot.PoolMaxSets.back(), slot.NextMaxSets, slot.NextSamplers);
    CreateDescriptorPoolInto(slot, dev.Device);

    VkDescriptorSet set = TryAllocateDescriptorSet(dev.Device, slot.Pools.back(), layout);
    if (set != VK_NULL_HANDLE)
    {
        ++slot.SetsAllocatedThisFrame;
        return set;
    }

    DMGE_LOG_ERROR("Vulkan: descriptor set allocation failed even after pool "
                   "growth - this draw will be skipped.");
    return VK_NULL_HANDLE;
}

void VulkanRendererAPI::WriteDescriptorSet(VkDescriptorSet set, VulkanShader& shader,
                                            uint32_t frameIndex)
{
    auto& dev = VulkanDevice::Get();
    VkDeviceSize blockSize = shader.GetUniformBlockSize();

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(1 + shader.GetSamplerCount());

    VkDescriptorBufferInfo uboInfo{};
    VkWriteDescriptorSet uboWrite{};
    if (blockSize > 0)
    {
        uboInfo.buffer = m_UniformBuffers[frameIndex];
        uboInfo.offset = 0;
        uboInfo.range  = blockSize;

        uboWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet           = set;
        uboWrite.dstBinding       = 0;
        uboWrite.dstArrayElement  = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        uboWrite.pBufferInfo      = &uboInfo;
        writes.push_back(uboWrite);
    }

    // Samplers: binding 1..N filled from the currently bound texture slots.
    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(shader.GetSamplerCount());
    for (uint32_t i = 0; i < shader.GetSamplerCount(); ++i)
    {
        VulkanTexture* tex = (i < kMaxBoundTextures) ? m_BoundTextures[i] : nullptr;
        VkDescriptorImageInfo info{};
        if (tex)
        {
            info.sampler   = tex->GetVkSampler();
            info.imageView = tex->GetVkImageView();
        }
        else
        {
            // Unbound sampler slot: bind the global 1x1 dummy so the
            // descriptor is valid (sampling returns black) instead of
            // leaving the binding unwritten, which triggers
            // "descriptor set encountered uninitialized binding"
            // validation and undefined sampling results.
            info.sampler   = m_DummySampler;
            info.imageView = m_DummyView;
        }
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(info);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet           = set;
        write.dstBinding       = i + 1; // samplers start at binding 1
        write.dstArrayElement  = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo       = &imageInfos.back();
        writes.push_back(write);
    }

    if (!writes.empty())
        vkUpdateDescriptorSets(dev.Device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
}

} // namespace DMGameEngine
