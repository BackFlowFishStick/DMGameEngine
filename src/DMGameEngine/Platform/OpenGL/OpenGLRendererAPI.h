/*
 * DMGameEngine - OpenGL Renderer API
 *
 * OpenGL implementation of the RendererAPI abstraction.
 * Issues backend-specific draw calls via GLAD/OpenGL.
 */

#pragma once

#include "DMGameEngine/Renderer/RendererAPI.h"

#include <glad/glad.h>

namespace DMGameEngine {

class DMGE_API OpenGLRendererAPI : public RendererAPI
{
public:
    void Init() override;
    void SetClearColor(const glm::vec4& color) override;
    void Clear() override;
    void SetViewport(int x, int y, int width, int height) override;
    void DrawIndexed(const VertexArray& vertexArray) override;
};

} // namespace DMGameEngine