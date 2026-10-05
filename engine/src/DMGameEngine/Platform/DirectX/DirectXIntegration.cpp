/*
 * DMGameEngine - DirectX Backend Integration Hook (implementation)
 */

#include "DMGameEngine/Platform/DirectX/DirectXIntegration.h"
#include "DMGameEngine/Platform/DirectX/D3D11RendererAPI.h"
#include "DMGameEngine/Platform/DirectX/DirectXGraphicsContext.h"

namespace DMGameEngine {
namespace DirectX {

DM::Scope<RendererAPI> CreateDirectXRendererAPI()
{
    return DM::CreateScope<D3D11RendererAPI>();
}

DM::Scope<GraphicsContext> CreateDirectXGraphicsContext(void* hwnd)
{
    return DM::CreateScope<DirectXGraphicsContext>(hwnd);
}

} // namespace DirectX
} // namespace DMGameEngine
