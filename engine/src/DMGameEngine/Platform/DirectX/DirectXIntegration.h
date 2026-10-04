/*
 * DMGameEngine - DirectX Backend Integration Hook
 *
 * Stage-A factory for the Direct3D 11 RendererAPI implementation. The
 * renderer abstraction's own factories (RendererAPI::Create etc.) live in
 * files owned by the render agent, so the actual switch wiring is done by
 * the integrator (see documents/DIRECTX_BACKEND_DESIGN.md §7 for the exact
 * 3-line change). Until then, tests and tooling construct the backend
 * through this factory (or the backend classes directly).
 *
 * Exposed via DMGameEngine.h under the DMGE_D3D11 gate (R9).
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

namespace DMGameEngine {

class RendererAPI; // forward declaration - no renderer headers leak here

namespace DirectX {

// Returns a freshly created, uninitialized D3D11RendererAPI. Call Init()
// (headless-capable: hardware adapter with WARP fallback) before use.
DMGE_API DM::Scope<RendererAPI> CreateDirectXRendererAPI();

} // namespace DirectX
} // namespace DMGameEngine
