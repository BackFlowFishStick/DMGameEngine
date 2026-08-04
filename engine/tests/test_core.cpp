/*
 * DMGameEngine - core unit tests (pure logic, no GPU)
 *
 * Covers header-only utilities that have no graphics-backend dependency, so the
 * suite runs headless on CI. Add backend-level smoke tests (offscreen
 * Init->BeginScene->Submit->EndScene->Shutdown) under a separate target gated
 * on DMGE_VULKAN_BACKEND / a GL context once the harness supports headless GL.
 */

#include <gtest/gtest.h>

#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Renderer/Shader.h"

using namespace DMGameEngine;

// ── Timestep ──────────────────────────────────────────────────────

TEST(Timestep, ConvertsSecondsAndMilliseconds)
{
    Timestep ts(1.5f);
    EXPECT_FLOAT_EQ(ts.GetSeconds(), 1.5f);
    EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 1500.0f);
}

TEST(Timestep, ImplicitFloatConversion)
{
    Timestep ts(0.25f);
    float seconds = ts;
    EXPECT_FLOAT_EQ(seconds, 0.25f);
}

TEST(Timestep, ZeroIsZero)
{
    Timestep ts(0.0f);
    EXPECT_FLOAT_EQ(ts.GetSeconds(), 0.0f);
    EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 0.0f);
}

// ── Shader data types / buffer layout ────────────────────────────

TEST(ShaderDataType, SizeBytes)
{
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float),  4u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float2), 8u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float3), 12u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float4), 16u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Int),    4u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Bool),   1u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Mat3),   4u * 3 * 3);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Mat4),   4u * 4 * 4);
}

TEST(BufferLayout, ComputesStrideAndOffsets)
{
    BufferLayout layout = {
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float2, "a_TexCoord" },
    };
    EXPECT_EQ(layout.GetStride(), 12u + 8u);
    ASSERT_EQ(layout.GetElements().size(), 2u);
    EXPECT_EQ(layout.GetElements()[0].Offset, 0u);
    EXPECT_EQ(layout.GetElements()[1].Offset, 12u);
}