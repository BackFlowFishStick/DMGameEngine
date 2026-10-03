#include "EditorLayer.h"
#include <DMGameEngine/Core/Log.h>
#include <DMGameEngine/Scene/SceneSerializer.h>
#include <DMGameEngine/Scene/Components/Components.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <filesystem>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

using namespace DMGameEngine;

struct AABBd { glm::vec3 min, max; };

static bool MeshLocalBounds(const Mesh& mesh, AABBd& out) {
    const auto& elems = mesh.Layout.GetElements();
    const uint32_t stride = mesh.Layout.GetStride();
    uint32_t posOffset = UINT32_MAX;
    for (const auto& el : elems)
        if (el.Name == "a_Position") { posOffset = el.Offset; break; }
    if (posOffset == UINT32_MAX || stride == 0 || mesh.Vertices.empty()) return false;
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    const float* data = mesh.Vertices.data();
    const uint32_t fStride = stride / 4;
    const uint32_t fPos = posOffset / 4;
    const uint32_t count = static_cast<uint32_t>(mesh.Vertices.size()) / fStride;
    for (uint32_t i = 0; i < count; ++i) {
        const float* p = data + i * fStride + fPos;
        glm::vec3 v(p[0], p[1], p[2]);
        mn = glm::min(mn, v);
        mx = glm::max(mx, v);
    }
    out = { mn, mx };
    return true;
}

static bool RayAABB(const glm::vec3& o, const glm::vec3& d,
                    const glm::vec3& bmin, const glm::vec3& bmax, float& tHit) {
    float tmin = 0.0f, tmax = FLT_MAX;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-6f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) return false;
        } else {
            float inv = 1.0f / d[i];
            float t1 = (bmin[i] - o[i]) * inv;
            float t2 = (bmax[i] - o[i]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}

EditorLayer::EditorLayer()
    : Layer("EditorLayer", LayerType::Tool) {}

void EditorLayer::OnAttach() {
    ImGuiIO& io = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    // The engine enables multi-viewport (ViewportsEnable) which makes
    // io.MousePos global-screen while some ImGui window rects use a different
    // origin -> ImGuizmo hit-testing breaks (mouse vs rect mismatch). The
    // editor docks everything in the main window, so disable multi-viewport.
    // Docking still works; only OS-level detached windows are turned off.
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    DMGE_CLIENT_INFO("EditorLayer attached");
    m_Log.OnAttach();
}

void EditorLayer::OnDetach() { m_Log.OnDetach(); }

void EditorLayer::OnUpdate(Timestep ts) {
    if (auto* cam = m_Scene.GetCamera())
        cam->SetEnabled(m_ViewportFocused && m_ViewportHovered);
    m_Scene.OnUpdate(ts);
}

void EditorLayer::OnEvent(Event& e) {
    if (m_Scene.GetCamera())
        m_Scene.GetCamera()->OnEvent(e);
}

void EditorLayer::OnRender() {
    m_Scene.Render();
}

void EditorLayer::OnImGuiRender() {
    ImGuizmo::BeginFrame();
    DrawDockspace();
    DrawMenuBar();
    DrawViewport();
    DrawHierarchy();
    DrawInspector();
    DrawSystems();
    DrawAssetBrowser();
    m_Log.OnImGuiRender();
}

// ── Play mode isolation (stage 4) ───────────────────────────────

void EditorLayer::BeginPlay() {
    if (m_Scene.IsPlaying()) return;
    // Remember the selection by UUID so it can be restored on the edit scene
    // after Stop (runtime entity ids differ between the two scenes).
    Scene* s = m_Scene.GetEditScene();
    m_SelectedUUID = 0;
    if (s && m_Selected != NullEntity && s->HasComponent<IDComponent>(m_Selected))
        m_SelectedUUID = s->GetComponent<IDComponent>(m_Selected).UUID;
    m_Scene.EnterPlayMode();
    DMGE_CLIENT_INFO("Play mode entered - edit state snapshotted, changes during play will be discarded on Stop");
}

void EditorLayer::EndPlay() {
    if (!m_Scene.IsPlaying()) return;
    m_Scene.ExitPlayMode();   // discards the play copy, restores the edit scene
    m_Selected = NullEntity;
    Scene* s = m_Scene.GetEditScene();
    if (s && m_SelectedUUID != 0) {
        s->GetRegistry().view<IDComponent>().each([&](auto eh, IDComponent& idc) {
            if (idc.UUID == m_SelectedUUID)
                m_Selected = static_cast<Entity>(eh);
        });
    }
    m_SelectedUUID = 0;
    DMGE_CLIENT_INFO("Play mode stopped - edit state restored");
}

void EditorLayer::DrawDockspace() {
#ifdef IMGUI_HAS_DOCK
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##EditorDockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiID dockspaceId = ImGui::GetID("DMEditorDock");
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0),
                     ImGuiDockNodeFlags_PassthruCentralNode);
    // Unity-style default layout on first launch: left hierarchy, center
    // viewport, right inspector, bottom asset browser. ImGui persists the
    // layout to imgui.ini afterwards, so the user can rearrange freely.
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);
        ImGuiID dockMain = dockspaceId;
        ImGuiID dockLeft  = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left,  0.18f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
        ImGuiID dockDown  = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down,  0.28f, nullptr, &dockMain);
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector",        dockRight);
        ImGui::DockBuilderDockWindow("Systems",          dockRight);
        ImGui::DockBuilderDockWindow("Asset Browser",    dockDown);
        ImGui::DockBuilderDockWindow("Log",               dockDown);
        ImGui::DockBuilderDockWindow("Viewport",         dockMain);
        ImGui::DockBuilderFinish(dockspaceId);
    }
    ImGui::End();
#endif
}

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                m_Scene.NewScene();
                m_Selected = NullEntity;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (auto* s = m_Scene.GetScene())
                    SceneSerializer::Save(*s, m_ScenePath);
            }
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) {
                if (auto* s = m_Scene.GetScene()) {
                    SceneSerializer::Load(*s, m_ScenePath);
                    m_Selected = NullEntity;
                }
            }
            ImGui::Separator();
            char pathBuf[512];
            std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_ScenePath.c_str());
            if (ImGui::InputText("Scene path", pathBuf, 512))
                m_ScenePath = pathBuf;
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4"))
                Application::Get().Quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Entity")) {
            // Play-mode isolation: entity CRUD only in edit mode (the play
            // copy is discarded on Stop - editing it would mislead the user).
            if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+A", false, !m_Scene.IsPlaying())) {
                if (auto* s = m_Scene.GetScene())
                    m_Selected = s->CreateEntity("Entity");
            }
            if (m_Selected != NullEntity && ImGui::MenuItem("Delete", "Del", false, !m_Scene.IsPlaying())) {
                if (auto* s = m_Scene.GetScene()) {
                    s->DestroyEntity(m_Selected);
                    m_Selected = NullEntity;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            if (m_Scene.IsPlaying()) {
                if (ImGui::MenuItem(m_Scene.IsPaused() ? "Resume" : "Pause", "F5"))
                    m_Scene.SetPaused(!m_Scene.IsPaused());
                if (ImGui::MenuItem("Stop", "F6"))
                    EndPlay();
            } else if (ImGui::MenuItem("Play", "F5")) {
                BeginPlay();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::DrawViewport() {
    ImGui::Begin("Viewport");
    // Gizmo mode toolbar (screen buttons + 1/2/3/4 keys; does not conflict
    // with camera WASD/QE dolly).
    {
        auto modeBtn = [&](const char* label, int v) {
            bool active = (m_GizmoType == v);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label)) m_GizmoType = v;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        modeBtn("Translate", 0);
        modeBtn("Rotate", 1);
        modeBtn("Scale", 2);
        modeBtn("None", -1);
        ImGui::NewLine();
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_GizmoType = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) m_GizmoType = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) m_GizmoType = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_4)) m_GizmoType = -1;
    }
    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_ViewportSize = glm::vec2(avail.x, avail.y);

    uint32_t w = static_cast<uint32_t>(avail.x);
    uint32_t h = static_cast<uint32_t>(avail.y);
    if (w > 0 && h > 0)
        m_Scene.Resize(w, h);

    // Camera control via ImGui mouse state. ImGuiLayer intercepts engine mouse
    // events over the viewport (io.WantCaptureMouse) so EditorCameraController::OnEvent
    // never receives them - drive the camera directly here instead.
    // Right-drag = orbit, Middle-drag = pan, Wheel = zoom. Left = pick/gizmo.
    if (m_ViewportHovered && m_ViewportFocused && !ImGuizmo::IsUsing()) {
        auto* cam = m_Scene.GetCamera();
        if (cam) {
            ImGuiIO& io = ImGui::GetIO();
            const glm::vec2 d(io.MouseDelta.x, io.MouseDelta.y);
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                cam->SetYaw(cam->GetYaw() - d.x * cam->GetRotateSpeed());
                cam->SetPitch(cam->GetPitch() + d.y * cam->GetRotateSpeed());
            } else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                glm::vec3 fwd = glm::normalize(cam->GetTarget() - cam->GetPosition());
                glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
                glm::vec3 up  = glm::normalize(glm::cross(rgt, fwd));
                const float s = cam->GetPanSpeed() * cam->GetDistance();
                glm::vec3 t = cam->GetTarget();
                t -= rgt * d.x * s;
                t += up  * d.y * s;
                cam->SetTarget(t);
            }
            if (io.MouseWheel != 0.0f)
                cam->SetDistance(cam->GetDistance() - io.MouseWheel * cam->GetZoomSpeed());
        }
    }

    if (avail.x > 0.0f && avail.y > 0.0f && m_Scene.GetTarget() &&
        m_Scene.GetTarget()->GetColorAttachmentCount() > 0) {
        ImTextureID texID = (ImTextureID)(uintptr_t)
            m_Scene.GetTarget()->GetColorAttachment(0)->GetRendererID();
        ImGui::Image(texID, avail, ImVec2(0, 1), ImVec2(1, 0));

        // Mouse pick: click the viewport to select the nearest mesh entity.
        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
            ImVec2 imMin = ImGui::GetItemRectMin();
            ImVec2 imMax = ImGui::GetItemRectMax();
            ImVec2 mp = ImGui::GetMousePos();
            if (mp.x >= imMin.x && mp.x <= imMax.x && mp.y >= imMin.y && mp.y <= imMax.y) {
                const float x = (mp.x - imMin.x) / (imMax.x - imMin.x);
                const float y = (mp.y - imMin.y) / (imMax.y - imMin.y);
                const float ndcX = x * 2.0f - 1.0f;
                const float ndcY = 1.0f - y * 2.0f;
                if (auto* cam = m_Scene.GetCamera()) {
                    glm::mat4 invVP = glm::inverse(cam->GetCamera().GetViewProjection());
                    glm::vec4 np = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                    glm::vec4 fp = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                    np /= np.w; fp /= fp.w;
                    glm::vec3 ro(np), rd = glm::normalize(glm::vec3(fp) - ro);
                    Scene* sc = m_Scene.GetScene();
                    float bestT = FLT_MAX;
                    Entity best = NullEntity;
                    auto& reg = sc->GetRegistry();
                    reg.view<MeshComponent>().each([&](auto eh, MeshComponent&) {
                        Entity e = static_cast<Entity>(eh);
                        if (!sc->HasComponent<TransformComponent>(e)) return;
                        auto& mc = sc->GetComponent<MeshComponent>(e);
                        auto& tc = sc->GetComponent<TransformComponent>(e);
                        if (!mc.Mesh) return;
                        AABBd lb;
                        if (!MeshLocalBounds(*mc.Mesh, lb)) return;
                        glm::vec3 corners[8] = {
                            {lb.min.x,lb.min.y,lb.min.z}, {lb.max.x,lb.min.y,lb.min.z},
                            {lb.min.x,lb.max.y,lb.min.z}, {lb.max.x,lb.max.y,lb.min.z},
                            {lb.min.x,lb.min.y,lb.max.z}, {lb.max.x,lb.min.y,lb.max.z},
                            {lb.min.x,lb.max.y,lb.max.z}, {lb.max.x,lb.max.y,lb.max.z}
                        };
                        glm::vec3 wmn(FLT_MAX), wmx(-FLT_MAX);
                        for (const auto& c : corners) {
                            glm::vec3 w = glm::vec3(tc.WorldMatrix * glm::vec4(c, 1.0f));
                            wmn = glm::min(wmn, w); wmx = glm::max(wmx, w);
                        }
                        float t;
                        if (RayAABB(ro, rd, wmn, wmx, t) && t < bestT) { bestT = t; best = e; }
                    });
                    m_Selected = best;
                }
            }
        }

        // Gizmo editing only in edit mode: writing transforms during play
        // would mutate the play copy that gets discarded on Stop, and the
        // inspector is read-only then - keep them consistent.
        if (!m_Scene.IsPlaying() && m_Selected != NullEntity && m_GizmoType >= 0) {

            Scene* sc = m_Scene.GetScene();
            if (sc && sc->HasComponent<TransformComponent>(m_Selected)) {
                // Draw into the Viewport window's own draw list. ImGuizmo's
                // IsHoveringWindow() resolves the owning window from
                // gContext.mDrawList->_OwnerName to set mbMouseOver (hit-testing);
                // ForegroundDrawList has no owning window, so mbMouseOver was always
                // false and IsOver()/IsUsing() silently failed. Drawing here (after
                // ImGui::Image) also keeps the gizmo above the viewport image.
                ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                ImGuizmo::SetOrthographic(false);
                ImVec2 rmin = ImGui::GetItemRectMin();
                ImVec2 rmax = ImGui::GetItemRectMax();
                ImGuizmo::SetRect(rmin.x, rmin.y, rmax.x - rmin.x, rmax.y - rmin.y);
                auto* cam = m_Scene.GetCamera();
                glm::mat4 view = cam->GetCamera().GetView();
                glm::mat4 proj = cam->GetCamera().GetProjection();
                auto& tc = sc->GetComponent<TransformComponent>(m_Selected);
                glm::mat4 matrix = tc.WorldMatrix;
                ImGuizmo::OPERATION op = (m_GizmoType == 0) ? ImGuizmo::TRANSLATE
                                         : (m_GizmoType == 1) ? ImGuizmo::ROTATE
                                                               : ImGuizmo::SCALE;
                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                     op, ImGuizmo::LOCAL, glm::value_ptr(matrix));
                float t[3], rotDeg[3], sca[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), t, rotDeg, sca);
                tc.Translation = glm::vec3(t[0], t[1], t[2]);
                tc.RotationEuler = glm::radians(glm::vec3(rotDeg[0], rotDeg[1], rotDeg[2]));
                tc.Scale = glm::vec3(sca[0], sca[1], sca[2]);
                tc.Dirty = true;
                sc->MarkSubtreeDirty(m_Selected);
            }
        }
    }
    ImGui::End();
}

void EditorLayer::DrawHierarchy() {
    ImGui::Begin("Scene Hierarchy");
    Scene* s = m_Scene.GetScene();
    if (s) {
        auto& reg = s->GetRegistry();
        reg.view<TagComponent>().each([&](auto eHandle, auto&) {
            Entity e = static_cast<Entity>(eHandle);
            bool isRoot = true;
            if (s->HasComponent<TransformComponent>(e))
                isRoot = s->GetComponent<TransformComponent>(e).Parent == NullEntity;
            if (isRoot)
                DrawEntityNode(e);
        });
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()
            && !ImGui::IsAnyItemHovered())
            m_Selected = NullEntity;
    }
    ImGui::End();
}

void EditorLayer::DrawEntityNode(Entity e) {
    Scene* s = m_Scene.GetScene();
    if (!s || !s->HasComponent<TagComponent>(e))
        return;
    auto& tag = s->GetComponent<TagComponent>(e).Tag;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool hasChildren = false;
    if (s->HasComponent<TransformComponent>(e))
        hasChildren = s->GetComponent<TransformComponent>(e).FirstChild != NullEntity;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (e == m_Selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>((uintptr_t)e),
        flags, "%s", tag.c_str());
    if (ImGui::IsItemClicked())
        m_Selected = e;
    if (open) {
        if (s->HasComponent<TransformComponent>(e)) {
            Entity child = s->GetComponent<TransformComponent>(e).FirstChild;
            while (child != NullEntity) {
                DrawEntityNode(child);
                if (s->HasComponent<TransformComponent>(child))
                    child = s->GetComponent<TransformComponent>(child).NextSibling;
                else
                    break;
            }
        }
        ImGui::TreePop();
    }
}

void EditorLayer::DrawInspector() {
    ImGui::Begin("Inspector");
    Scene* s = m_Scene.GetScene();
    if (m_Selected != NullEntity && s &&
        s->GetRegistry().valid(static_cast<entt::entity>(m_Selected))) {
        DrawComponents(m_Selected);
    } else {
        ImGui::TextDisabled("No entity selected");
    }
    ImGui::End();
}

void EditorLayer::DrawComponents(Entity e) {
    Scene* s = m_Scene.GetScene();

    // Play-mode isolation: the entity under inspection belongs to the play
    // copy, which is discarded on Stop - make everything read-only.
    const bool readOnly = m_Scene.IsPlaying();
    if (readOnly) {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.3f, 1.0f),
            "Play mode - read-only (changes are discarded on Stop)");
        ImGui::Separator();
        ImGui::BeginDisabled(true);
    }

    if (s->HasComponent<TagComponent>(e)) {
        auto& tag = s->GetComponent<TagComponent>(e).Tag;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", tag.c_str());
        if (ImGui::InputText("Tag", buf, 256))
            tag = buf;
    }
    if (s->HasComponent<IDComponent>(e)) {
        auto& id = s->GetComponent<IDComponent>(e);
        ImGui::Text("UUID: %llu", static_cast<unsigned long long>(id.UUID));
    }
    if (s->HasComponent<TransformComponent>(e)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tc = s->GetComponent<TransformComponent>(e);
            bool changed = false;
            changed |= ImGui::DragFloat3("Translation", glm::value_ptr(tc.Translation), 0.1f);
            glm::vec3 rotDeg = glm::degrees(tc.RotationEuler);
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotDeg), 0.5f)) {
                tc.RotationEuler = glm::radians(rotDeg);
                changed = true;
            }
            changed |= ImGui::DragFloat3("Scale", glm::value_ptr(tc.Scale), 0.1f, 0.01f, 100.0f);
            if (changed) { tc.Dirty = true; s->MarkSubtreeDirty(e); }
        }
    }
    if (s->HasComponent<CameraComponent>(e)) {
        if (ImGui::CollapsingHeader("Camera")) {
            auto& cc = s->GetComponent<CameraComponent>(e);
            ImGui::Checkbox("Primary", &cc.Primary);
            ImGui::Checkbox("Fixed Aspect Ratio", &cc.FixedAspectRatio);
            auto& cam = cc.Camera;
            const char* items[] = {"Perspective", "Orthographic"};
            int idx = static_cast<int>(cam.GetProjectionType());
            if (ImGui::Combo("Projection", &idx, items, 2))
                cam.SetProjectionType(static_cast<SceneCamera::ProjectionType>(idx));
            if (cam.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
                float fov = cam.GetPerspectiveVerticalFOV();
                if (ImGui::DragFloat("Vertical FOV", &fov, 0.5f, 1.0f, 179.0f))
                    cam.SetPerspectiveVerticalFOV(fov);
            }
        }
    }
    if (s->HasComponent<LightComponent>(e)) {
        if (ImGui::CollapsingHeader("Light")) {
            auto& lc = s->GetComponent<LightComponent>(e);
            const char* types[] = {"Directional", "Point", "Spot"};
            int idx = static_cast<int>(lc.LightType);
            if (ImGui::Combo("Type", &idx, types, 3))
                lc.LightType = static_cast<LightComponent::Type>(idx);
            ImGui::ColorEdit3("Color", glm::value_ptr(lc.Color));
            ImGui::DragFloat("Intensity", &lc.Intensity, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Ambient", &lc.AmbientIntensity, 0.01f, 0.0f, 5.0f);
        }
    }
    if (s->HasComponent<MeshComponent>(e)) {
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mc = s->GetComponent<MeshComponent>(e);
            ImGui::Text("Mesh: %s", mc.Mesh ? "loaded" : "(null)");
            if (mc.Mesh)
                ImGui::Text("SubMeshes: %llu",
                    static_cast<unsigned long long>(mc.Mesh->SubMeshes.size()));
            ImGui::Text("Material overrides: %llu",
                static_cast<unsigned long long>(mc.MaterialOverrides.size()));
            ImGui::Button("Drop [mesh] asset here", ImVec2(-1, 0));
            if (ImGui::BeginDragDropTarget()) {
                if (auto* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    std::string path((const char*)pl->Data);  // to null terminator
                    auto ext = std::filesystem::path(path).extension().string();
                    if (ext == ".mesh" || ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                        auto mesh = AssetManager::Get().Load<Mesh>(path);
                        if (mesh) {
                            mc.Mesh = mesh;
                            AABBd lb;
                            if (MeshLocalBounds(*mesh, lb)) {
                                // fbx models vary wildly in scale (e.g. this one is 200 units).
                                // Normalize so max extent ~= 2 units -> visible at default camera.
                                float maxExt = std::max({lb.max.x - lb.min.x, lb.max.y - lb.min.y, lb.max.z - lb.min.z});
                                if (maxExt > 0.0001f && s->HasComponent<TransformComponent>(e)) {
                                    float sc = 2.0f / maxExt;
                                    auto& tc = s->GetComponent<TransformComponent>(e);
                                    tc.Scale = {sc, sc, sc};
                                    tc.Dirty = true;
                                    s->MarkSubtreeDirty(e);
                                }
                            }
                            mc.MaterialOverrides.clear();
                            // assimp imports carry no material -> assign a default
                            // Blinn-Phong material so the mesh actually renders.
                            auto shader = AssetManager::Get().Load<Shader>(
                                "D:/CPPPractices/DMGameEngine/engine/shaders/BlinnPhong.glsl");
                            if (shader) {
                                auto baseMat = DM::CreateRef<Material>(shader);
                                baseMat->SetFloat3("u_AlbedoColor", {0.8f, 0.8f, 0.85f});
                                baseMat->SetFloat("u_SpecularStrength", 0.5f);
                                baseMat->SetFloat("u_Shininess", 64.0f);
                                baseMat->SetInt("u_UseTexture", 0);
                                mc.MaterialOverrides.resize(mesh->SubMeshes.size());
                                for (auto& ov : mc.MaterialOverrides)
                                    ov = DM::CreateRef<MaterialInstance>(baseMat);
                            }
                            DMGE_CLIENT_INFO("Loaded mesh into entity: {0}", path);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

    // Add / Remove Component (runtime attach/detach).
    ImGui::Separator();
    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!s->HasComponent<CameraComponent>(e) && ImGui::MenuItem("Camera"))
            s->AddComponent<CameraComponent>(e);
        if (!s->HasComponent<LightComponent>(e) && ImGui::MenuItem("Light"))
            s->AddComponent<LightComponent>(e);
        if (!s->HasComponent<MeshComponent>(e) && ImGui::MenuItem("Mesh"))
            s->AddComponent<MeshComponent>(e);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove..."))
        ImGui::OpenPopup("RemoveComponentPopup");
    if (ImGui::BeginPopup("RemoveComponentPopup")) {
        if (s->HasComponent<CameraComponent>(e) && ImGui::MenuItem("Camera"))
            s->RemoveComponent<CameraComponent>(e);
        if (s->HasComponent<LightComponent>(e) && ImGui::MenuItem("Light"))
            s->RemoveComponent<LightComponent>(e);
        if (s->HasComponent<MeshComponent>(e) && ImGui::MenuItem("Mesh"))
            s->RemoveComponent<MeshComponent>(e);
        ImGui::EndPopup();
    }

    if (readOnly)
        ImGui::EndDisabled();
}

void EditorLayer::DrawSystems() {
    ImGui::Begin("Systems");
    Scene* s = m_Scene.GetScene();
    if (s) {
        const auto& systems = s->GetSystems();
        ImGui::Text("Registered systems: %llu",
            static_cast<unsigned long long>(systems.size()));
        ImGui::Separator();
        int i = 0;
        for (const auto& sys : systems) {
            ImGui::Text("%d. %s", i++, sys ? sys->GetName() : "(null)");
        }
        ImGui::Separator();
        ImGui::Text("Play mode: %s",
            m_Scene.IsPlaying() ? (m_Scene.IsPaused() ? "PAUSED" : "PLAYING") : "Editing");
        if (m_Scene.IsPlaying()) {
            if (ImGui::Button(m_Scene.IsPaused() ? "Resume##sys" : "Pause##sys"))
                m_Scene.SetPaused(!m_Scene.IsPaused());
            ImGui::SameLine();
            if (ImGui::Button("Stop##sys"))
                EndPlay();
        } else if (ImGui::Button("Play##sys")) {
            BeginPlay();
        }
    }
    ImGui::End();
}

void EditorLayer::DrawAssetBrowser() {
    ImGui::Begin("Asset Browser");
    namespace fs = std::filesystem;
    std::error_code ec;

    auto drawDir = [&](const char* root, const char* title) {
        if (!fs::exists(root, ec)) return;
        ImGui::Separator();
        ImGui::TextDisabled("%s", title);
        ImGui::Separator();
        // recursive: lists files in subdirectories too (e.g. assets/models/*.fbx).
        for (auto& it : fs::recursive_directory_iterator(root, ec)) {
            if (!it.is_regular_file()) continue;
            auto path = it.path().string();
            auto rel = fs::relative(it.path(), root, ec).string();
            auto ext = it.path().extension().string();
            const char* icon = "[file]";
            if (ext == ".mesh" || ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") icon = "[mesh]";
            else if (ext == ".glsl") icon = "[shader]";
            else if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds") icon = "[tex]";
            else if (ext == ".mat") icon = "[mat]";
            std::string label = std::string(icon) + "  " + rel;
            ImGui::PushID(path.c_str());
            // full-width row; clear, not cramped.
            if (ImGui::Selectable(label.c_str(), m_SelectedAsset == path))
                m_SelectedAsset = path;
            // Drag source: carries the asset path to MeshComponent drop target.
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);
                ImGui::Text("%s %s", icon, rel.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
    };

    drawDir("D:/CPPPractices/DMGameEngine/editor/assets", "Assets (editor/assets)");
    drawDir("D:/CPPPractices/DMGameEngine/engine/shaders", "Shaders (engine/shaders)");

    ImGui::Separator();
    if (!m_SelectedAsset.empty())
        ImGui::TextWrapped("Selected: %s", m_SelectedAsset.c_str());
    else
        ImGui::TextDisabled("Drag a [mesh] onto a MeshComponent to load it.");
    ImGui::End();
}
