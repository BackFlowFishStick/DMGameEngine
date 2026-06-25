/*
 * DMGameEngine - Public API Entry Header
 *
 * Game projects should include this single header to access
 * the full engine API:
 *
 *   #include <DMGameEngine/DMGameEngine.h>
 */

#pragma once

// ── Core ─────────────────────────────────────────────────────────
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Layer.h"
#include "DMGameEngine/Core/LayerStack.h"
#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Window.h"

// ── Events ───────────────────────────────────────────────────────
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Events/ApplicationEvent.h"
