/*
 * DMGameEngine - Direct3D 11 Texture2D
 *
 * D3D11 implementation of the Texture2D abstraction: an ID3D11Texture2D
 * plus an ID3D11ShaderResourceView. Textures are created with both the
 * SHADER_RESOURCE and (for color formats) RENDER_TARGET bind flags so they
 * can double as framebuffer attachments and mip-generation targets.
 *
 * Unit semantics: Bind(slot) registers this texture's SRV in the backend's
 * GL-style texture unit table (D3D11Common) and issues an immediate
 * PSSetShaderResources - mirroring glBindTexture(GL_TEXTURE0 + slot).
 * D3D11Shader::Bind() later resolves sampler uniforms against the table.
 *
 * Image orientation: unlike the OpenGL backend (GL origin bottom-left),
 * D3D11's origin is top-left, so file loads do NOT flip vertically.
 */

#pragma once

#include "DMGameEngine/Renderer/Texture2D.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <string>
#include <string_view>

namespace DMGameEngine {

// C4251 ("needs dll-interface for members") is intentionally suppressed for
// the D3D11 backend classes: members are COM pointers (no CRT state),
// header-only template types (ComPtr/glm), or private STL members that are
// only ever touched inside the engine DLL (same CRT by construction) - the
// pattern the OpenGL backend's exported classes already accept (kb/KB-02).
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

class DMGE_API D3D11Texture2D : public Texture2D
{
public:
    explicit D3D11Texture2D(const Texture2DSpecification& spec);
    explicit D3D11Texture2D(std::string_view filepath);
    // Adopts an existing RT texture + SRV (used by D3D11FrameBuffer for its
    // color attachments, which must stay render-target bound).
    D3D11Texture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv,
                   const Texture2DSpecification& spec);
    ~D3D11Texture2D() override;

    void Bind(uint32_t slot = 0) const override;
    void Unbind()                 const override;

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    const Texture2DSpecification& GetSpecification() const override { return m_Spec; }

    void SetData(void* data, uint32_t size) override;
    void GenerateMipmaps() override;

private:
    void Create(const void* initialData);

    Texture2DSpecification m_Spec;
    std::string m_FilePath;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
    uint32_t m_RendererID = 0;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
