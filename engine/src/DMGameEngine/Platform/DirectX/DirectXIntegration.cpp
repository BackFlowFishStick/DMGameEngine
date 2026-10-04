/*
 * DMGameEngine - DirectX Backend Integration Hook (implementation)
 */

#include "DMGameEngine/Platform/DirectX/DirectXIntegration.h"
#include "DMGameEngine/Platform/DirectX/D3D11RendererAPI.h"

namespace DMGameEngine {
namespace DirectX {

DM::Scope<RendererAPI> CreateDirectXRendererAPI()
{
    return DM::CreateScope<D3D11RendererAPI>();
}

} // namespace DirectX
} // namespace DMGameEngine
