/*
 * DMGameEngine - Precompiled Header (dmge_pch.h)
 *
 * Caches the parse of stable, heavy, universally-included headers into a
 * single compiler snapshot, so every C++ TU reuses it instead of re-parsing
 * ~2 MB of templates (glm ~1.3 MB + spdlog/fmt ~957 KB) per file.
 *
 * SCOPE RULES (see PRECOMPILED_HEADER.md for the full rationale):
 *   - ONLY C++ standard library + stable 3rd-party libs (glm, spdlog) here.
 *   - NO engine headers that change often: a PCH'd header change forces a
 *     full rebuild of every TU, so only put rarely-touched code here.
 *   - NO glad / GLFW / imgui / stb / nlohmann_json / assimp: they are used by
 *     only a few TUs and would bloat the .pch while polluting unrelated TUs.
 *   - NO Vulkan headers: the Vulkan backend is optional and glad <-> Vulkan
 *     headers conflict if both land in a shared precompiled snapshot.
 *
 * This file is force-included into every C++ TU by target_precompile_headers
 * (/FI on MSVC, -include on GCC/Clang). Source files MUST still #include what
 * they use - the PCH only accelerates parsing, it never replaces explicit
 * includes, so the code stays portable and the PCH can be disabled any time.
 */
#pragma once

// ── C++ Standard Library (high-frequency, immutable) ────────────
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// ── glm (OpenGL Mathematics) - heavy, template-dense, stable ─────
// Exactly the submodules the engine uses (matrix math, quaternions,
// value-pointer bridging for GLSL uniforms).
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// ── spdlog - heavy (~957 KB incl. bundled fmt), pulled via Log.h ──
// Log.h is the single most-included engine header (73% of TUs); caching
// spdlog here is the biggest win. The sinks match what Log.h includes.
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>