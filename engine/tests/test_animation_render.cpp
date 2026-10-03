/*
 * DMGameEngine - skinning render path headless tests (animation stage 1)
 *
 * The GPU itself cannot be exercised headlessly (kb/KB-07 K-014), and backend
 * objects (VertexArray) cannot even be constructed without a live context.
 * These tests therefore pin the CPU-side contracts of the per-draw palette
 * path: Renderable's palette fields (pointer semantics, defaults) and the
 * Shader::SetMat4Array default no-op (Vulkan/other backends stay loadable).
 * The actual u_BoneMatrices upload happens at Flush and is verified manually
 * - see the stage-1 report's manual verification steps.
 */
#include <gtest/gtest.h>

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/RenderQueue.h"
#include "DMGameEngine/Renderer/Shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace DMGameEngine;

namespace {

class NullShader : public Shader
{
public:
    void Bind() const override {}
    void Unbind() const override {}
    const std::string& GetName() const override { return m_Name; }
    void SetInt(std::string_view, int) override {}
    void SetIntArray(std::string_view, const int*, uint32_t) override {}
    void SetFloat(std::string_view, float) override {}
    void SetFloat2(std::string_view, const glm::vec2&) override {}
    void SetFloat3(std::string_view, const glm::vec3&) override {}
    void SetFloat4(std::string_view, const glm::vec4&) override {}
    void SetMat4(std::string_view, const glm::mat4&) override {}
    // SetMat4Array: inherited default no-op - exactly what this test pins.
private:
    std::string m_Name = "NullShader";
};

class AnimationRenderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static bool logReady = false;
        if (!logReady) { Log::Init(); logReady = true; }
    }
};

} // namespace

TEST_F(AnimationRenderTest, RenderablePaletteDefaultsToNull)
{
    // Static draws must never upload a palette: the fields default to
    // null/0 so the flush-time guard (palette && count > 0) skips them.
    Renderable r;
    EXPECT_EQ(r.BonePalette, nullptr);
    EXPECT_EQ(r.BonePaletteCount, 0u);
}

TEST_F(AnimationRenderTest, RenderablePaletteIsPointerNotCopy)
{
    // The palette is stored by pointer (per-draw upload stays cheap; the
    // contract is "valid from Submit until Flush"). Values written after the
    // Renderable is filled are the ones the flush would upload - pinning
    // that no copy is taken at submit time.
    glm::mat4 palette[1] = { glm::mat4(0.0f) };

    Renderable r;
    r.BonePalette = palette;
    r.BonePaletteCount = 1;

    palette[0] = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
    EXPECT_EQ((*r.BonePalette)[3], glm::vec4(5.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(r.BonePaletteCount, 1u);
}

TEST_F(AnimationRenderTest, ShaderSetMat4ArrayDefaultIsSafeNoOp)
{
    // Backends without skinning (Vulkan in stage 1) inherit the default
    // no-op: calling it through the Shader interface must be safe. The
    // OpenGL backend overrides it with glUniformMatrix4fv (verified manually
    // with BlinnPhongSkinned.glsl).
    NullShader shader;
    const glm::mat4 palette[2] = { glm::mat4(1.0f), glm::mat4(1.0f) };
    shader.SetMat4Array("u_BoneMatrices", palette, 2);   // must not crash
    SUCCEED();
}
