#include <DMGameEngine/DMGameEngine.h>
#include <DMGameEngine/Core/EntryPoint.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ──────────────────────────────────────────────────────────────────
// Backend selection: DMGE_API=D3D11 (env) or a -d3d11/--d3d11 command
// line argument switches the demo to the Direct3D 11 backend. Default
// (and everything else) stays OpenGL. K-015: EntryPoint.h's main()
// discards argc/argv, but the MSVC CRT keeps them in __argc/__argv.
// ──────────────────────────────────────────────────────────────────
#if defined(_MSC_VER)
extern "C" int    __argc;
extern "C" char** __argv;
#endif

static bool EqualsIgnoreCase(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

static bool WantDirectXBackend()
{
    if (const char* env = std::getenv("DMGE_API"))
    {
        if (EqualsIgnoreCase(env, "D3D11") || EqualsIgnoreCase(env, "DirectX"))
            return true;
    }
#if defined(_MSC_VER)
    for (int i = 1; i < __argc; ++i)
    {
        if (EqualsIgnoreCase(__argv[i], "-d3d11") || EqualsIgnoreCase(__argv[i], "--d3d11"))
            return true;
    }
#endif
    return false;
}

// ──────────────────────────────────────────────────────────────────
// CreateCubeMesh: 24-vertex cube with flat per-face normals + UVs.
// ──────────────────────────────────────────────────────────────────
static DM::Ref<DMGameEngine::Mesh> CreateCubeMesh()
{
    using namespace DMGameEngine;
    auto mesh = DM::CreateRef<Mesh>();
    mesh->Layout = BufferLayout{
        {ShaderDataType::Float3, "a_Position"},
        {ShaderDataType::Float3, "a_Normal"},
        {ShaderDataType::Float2, "a_TexCoords"},
    };
    auto v = [&mesh](float px, float py, float pz,
                     float nx, float ny, float nz,
                     float u,  float w)
    {
        mesh->Vertices.push_back(px); mesh->Vertices.push_back(py); mesh->Vertices.push_back(pz);
        mesh->Vertices.push_back(nx); mesh->Vertices.push_back(ny); mesh->Vertices.push_back(nz);
        mesh->Vertices.push_back(u);  mesh->Vertices.push_back(w);
    };
    // 6 faces, CCW from outside, 4 verts each.
    v( 1,-1, 1,  1, 0, 0, 0,0); v( 1,-1,-1,  1, 0, 0, 1,0); v( 1, 1,-1,  1, 0, 0, 1,1); v( 1, 1, 1,  1, 0, 0, 0,1);
    v(-1,-1,-1, -1, 0, 0, 0,0); v(-1,-1, 1, -1, 0, 0, 1,0); v(-1, 1, 1, -1, 0, 0, 1,1); v(-1, 1,-1, -1, 0, 0, 0,1);
    v(-1, 1, 1,  0, 1, 0, 0,0); v( 1, 1, 1,  0, 1, 0, 1,0); v( 1, 1,-1,  0, 1, 0, 1,1); v(-1, 1,-1,  0, 1, 0, 0,1);
    v(-1,-1,-1,  0,-1, 0, 0,0); v( 1,-1,-1,  0,-1, 0, 1,0); v( 1,-1, 1,  0,-1, 0, 1,1); v(-1,-1, 1,  0,-1, 0, 0,1);
    v(-1,-1, 1,  0, 0, 1, 0,0); v( 1,-1, 1,  0, 0, 1, 1,0); v( 1, 1, 1,  0, 0, 1, 1,1); v(-1, 1, 1,  0, 0, 1, 0,1);
    v( 1,-1,-1,  0, 0,-1, 0,0); v(-1,-1,-1,  0, 0,-1, 1,0); v(-1, 1,-1,  0, 0,-1, 1,1); v( 1, 1,-1,  0, 0,-1, 0,1);
    for (uint32_t face = 0; face < 6; ++face)
    {
        uint32_t base = face * 4;
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 1); mesh->Indices.push_back(base + 2);
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 2); mesh->Indices.push_back(base + 3);
    }
    SubMesh sub; sub.IndexOffset = 0; sub.IndexCount = 36;
    mesh->SubMeshes.push_back(sub);
    return mesh;
}

// ──────────────────────────────────────────────────────────────────
// LitCubeScene: 1500 ECS cubes + Blinn-Phong lighting (instanced).
// ──────────────────────────────────────────────────────────────────
// Blinn-Phong shader asset: GLSL for OpenGL (and Vulkan when it consumes
// GLSL-style sources), the HLSL twin for D3D11 (see
// Platform/DirectX/Shaders/BlinnPhongInstanced.hlsl - identical uniform
// names, `#type vertex/#type fragment` blocks). The directories are injected
// by game/CMakeLists.txt (worktree-relative); the fallbacks keep the file
// compilable outside CMake.
#ifndef DMGE_GAME_SHADER_DIR
#define DMGE_GAME_SHADER_DIR "D:/CPPPractices/DMGameEngine/engine/shaders"
#endif
#ifndef DMGE_GAME_HLSL_DIR
#define DMGE_GAME_HLSL_DIR "D:/CPPPractices/DMGameEngine/engine/src/DMGameEngine/Platform/DirectX/Shaders"
#endif

static const char* BlinnPhongShaderPath()
{
#if defined(DMGE_D3D11)
    if (DMGameEngine::Renderer::GetAPI() == DMGameEngine::Renderer::API::DirectX)
        return DMGE_GAME_HLSL_DIR "/BlinnPhongInstanced.hlsl";
#endif
    return DMGE_GAME_SHADER_DIR "/BlinnPhongInstanced.glsl";
}

class LitCubeScene : public DMGameEngine::DefaultSceneLayer
{
public:
    LitCubeScene() : DefaultSceneLayer("LitCubeScene") {}

    void OnAttach() override
    {
        using namespace DMGameEngine;
        DMGE_CLIENT_INFO("LitCubeScene Attached");
        RenderCommand::SetClearColor({0.03f, 0.03f, 0.05f, 1.0f});
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthFunc(DepthFunc::Less);
        RenderCommand::SetCullMode(CullMode::Back);

        m_Scene = DM::CreateRef<Scene>();
        m_Scene->AddSystem(DM::CreateRef<TransformSystem>(*m_Scene));
        m_Scene->AddSystem(DM::CreateRef<LightSystem>(*m_Scene));
        m_Scene->AddSystem(DM::CreateRef<MeshRenderSystem>(*m_Scene));

        auto cam = DM::CreateRef<EditorCameraController>(kFov, 1280.0f / 720.0f, 0.1f, 200.0f);
        cam->SetTarget({0.0f, 0.0f, 0.0f});
        cam->SetDistance(kCamDistance);
        cam->SetYaw(kCamYaw);
        cam->SetPitch(kCamPitch);
        SetCameraController(cam);

        auto shader = AssetManager::Get().Load<Shader>(BlinnPhongShaderPath());
        if (!shader) { DMGE_CLIENT_ERROR("Failed to load shader '{}'", BlinnPhongShaderPath()); return; }
        auto baseMat = DM::CreateRef<Material>(shader);
        baseMat->SetFloat3("u_AlbedoColor",      {0.85f, 0.85f, 0.88f});
        baseMat->SetFloat ("u_SpecularStrength",  0.5f);
        baseMat->SetFloat ("u_Shininess",         64.0f);
        baseMat->SetInt   ("u_UseTexture",        0);
        m_SharedMaterial = DM::CreateRef<MaterialInstance>(baseMat);

        // Light-visual shader (unlit color cubes). The inline sources follow
        // the active backend's language: GLSL for OpenGL, HLSL for D3D11.
#if defined(DMGE_D3D11)
        if (DMGameEngine::Renderer::GetAPI() == DMGameEngine::Renderer::API::DirectX)
        {
            m_UnlitShader = Shader::Create("UnlitLightVisual",
                R"HSLS(#pragma pack_matrix(column_major)
cbuffer UnlitConstants : register(b0)
{
    column_major float4x4 u_ViewProjection;
    column_major float4x4 u_Transform;
};

// K-025: varyings before SV_Position.
struct VSOutput
{
    float4 v_Position : SV_Position;
};

struct VSInput
{
    float3 a_Position : POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.v_Position = mul(u_ViewProjection, mul(u_Transform, float4(input.a_Position, 1.0)));
    return output;
}
)HSLS",
                R"HSLS(cbuffer UnlitPixelConstants : register(b0)
{
    float3 u_Color;
};

float4 PSMain() : SV_Target
{
    return float4(u_Color, 1.0);
}
)HSLS");
        }
        else
#endif
        {
            m_UnlitShader = Shader::Create("UnlitLightVisual",
                R"VS(#version 430 core
            layout(location=0) in vec3 a_Position;
            uniform mat4 u_ViewProjection;
            uniform mat4 u_Transform;
            void main() {
                gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
            })VS",
                R"FS(#version 430 core
            layout(location=0) out vec4 FragColor;
            uniform vec3 u_Color;
            void main() {
                FragColor = vec4(u_Color, 1.0);
            })FS");
        }

        m_CubeMesh = CreateCubeMesh();
        CreateLights();
        CreateCubes();
        DMGE_CLIENT_INFO("LitCubeScene: {} cubes + {} lights created", kCubeCount, 1 + kPointLightCount);
    }

    void OnUpdate(DMGameEngine::Timestep ts) override
    {
        using namespace DMGameEngine;
        for (auto& a : m_Anims)
        {
            auto& tc = m_Scene->GetComponent<TransformComponent>(a.entity);
            tc.Translation += a.velocity * ts.GetSeconds();
            for (int i = 0; i < 3; ++i)
            {
                if (tc.Translation[i] >  kHalfExtent[i]) tc.Translation[i] -= kSize[i];
                else if (tc.Translation[i] < -kHalfExtent[i]) tc.Translation[i] += kSize[i];
            }
            a.angle += a.rotSpeed * ts.GetSeconds();
            tc.RotationEuler = a.rotAxis * a.angle;
            tc.Dirty = true;
        }
        DefaultSceneLayer::OnUpdate(ts);
    }

    void OnDetach() override
    {
        m_Anims.clear(); m_SharedMaterial.reset(); m_CubeMesh.reset(); m_Scene.reset();
        DMGE_CLIENT_INFO("LitCubeScene shutting down");
    }

private:
    void CreateLights()
    {
        using namespace DMGameEngine;
        {
            auto e = m_Scene->CreateEntity("Sun");
            auto& lc = m_Scene->AddComponent<LightComponent>(e);
            lc.LightType = LightComponent::Type::Directional;
            lc.Color = {1.0f, 0.95f, 0.85f};
            lc.Intensity = 1.2f;
            lc.AmbientIntensity = 0.18f;
            glm::vec3 sunEuler(glm::radians(-35.0f), glm::radians(-30.0f), 0.0f);
            m_Scene->SetRotation(e, sunEuler);
            glm::mat4 rotMat = glm::mat4_cast(glm::quat(sunEuler));
            glm::vec3 forward = glm::normalize(glm::vec3(rotMat[2]));
            glm::vec3 sunPos  = forward * 38.0f;
            auto& tc = m_Scene->GetComponent<TransformComponent>(e);
            tc.Translation = sunPos; tc.Scale = {2.0f, 2.0f, 2.0f}; tc.Dirty = true;
            auto& mc = m_Scene->AddComponent<MeshComponent>(e);
            mc.Mesh = m_CubeMesh;
            auto sunMat = DM::CreateRef<Material>(m_UnlitShader);
            sunMat->SetFloat3("u_Color", {1.0f, 0.95f, 0.7f});
            // Deferred G-buffer pass uploads u_AlbedoColor (not u_Color):
            // without it the light visuals would lose their color.
            sunMat->SetFloat3("u_AlbedoColor", {1.0f, 0.95f, 0.7f});
            mc.MaterialOverrides.resize(1);
            mc.MaterialOverrides[0] = DM::CreateRef<MaterialInstance>(sunMat);
        }
        struct LightSpec { glm::vec3 pos; glm::vec3 color; float intensity; };
        const LightSpec specs[] = {
            {{ 12.0f,  4.0f,  0.0f}, {1.0f, 0.3f, 0.3f}, 3.0f},
            {{-12.0f,  4.0f,  0.0f}, {0.3f, 1.0f, 0.3f}, 3.0f},
            {{  0.0f,  4.0f, 12.0f}, {0.3f, 0.4f, 1.0f}, 3.0f},
            {{  0.0f,  4.0f, -12.0f}, {1.0f, 0.9f, 0.2f}, 3.0f},
        };
        for (int i = 0; i < kPointLightCount; ++i)
        {
            auto e = m_Scene->CreateEntity("PointLight_" + std::to_string(i));
            auto& lc = m_Scene->AddComponent<LightComponent>(e);
            lc.LightType = LightComponent::Type::Point;
            lc.Color = specs[i].color; lc.Intensity = specs[i].intensity;
            lc.Constant = 1.0f; lc.Linear = 0.07f; lc.Quadratic = 0.017f;
            m_Scene->SetTranslation(e, specs[i].pos);
            auto& tc = m_Scene->GetComponent<TransformComponent>(e);
            tc.Scale = {0.35f, 0.35f, 0.35f}; tc.Dirty = true;
            auto& mc = m_Scene->AddComponent<MeshComponent>(e);
            mc.Mesh = m_CubeMesh;
            auto lightMat = DM::CreateRef<Material>(m_UnlitShader);
            lightMat->SetFloat3("u_Color", specs[i].color);
            lightMat->SetFloat3("u_AlbedoColor", specs[i].color);
            mc.MaterialOverrides.resize(1);
            mc.MaterialOverrides[0] = DM::CreateRef<MaterialInstance>(lightMat);
        }
    }

    void CreateCubes()
    {
        using namespace DMGameEngine;
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> posX(-kHalfExtent[0], kHalfExtent[0]);
        std::uniform_real_distribution<float> posY(-kHalfExtent[1], kHalfExtent[1]);
        std::uniform_real_distribution<float> posZ(-kHalfExtent[2], kHalfExtent[2]);
        std::uniform_real_distribution<float> vel(-0.8f, 0.8f);
        std::uniform_real_distribution<float> axis(-1.0f, 1.0f);
        std::uniform_real_distribution<float> rotSpeed(kMinRotSpeed, kMaxRotSpeed);
        std::uniform_real_distribution<float> scale(kMinScale, kMaxScale);
        m_Anims.reserve(kCubeCount);
        for (uint32_t i = 0; i < kCubeCount; ++i)
        {
            auto e = m_Scene->CreateEntity("Cube_" + std::to_string(i));
            auto& tc = m_Scene->GetComponent<TransformComponent>(e);
            tc.Translation = {posX(rng), posY(rng), posZ(rng)};
            float s = scale(rng); tc.Scale = {s, s, s}; tc.Dirty = true;
            auto& mc = m_Scene->AddComponent<MeshComponent>(e);
            mc.Mesh = m_CubeMesh;
            mc.MaterialOverrides.resize(1);
            mc.MaterialOverrides[0] = m_SharedMaterial;
            CubeAnim a; a.entity = e;
            a.velocity = {vel(rng), vel(rng), vel(rng)};
            glm::vec3 ax(axis(rng), axis(rng), axis(rng));
            float axLen = glm::length(ax);
            a.rotAxis = axLen > 0.01f ? ax / axLen : glm::vec3(0, 1, 0);
            a.rotSpeed = rotSpeed(rng); a.angle = 0.0f;
            m_Anims.push_back(a);
        }
    }

    struct CubeAnim { DMGameEngine::Entity entity; glm::vec3 velocity; glm::vec3 rotAxis; float rotSpeed; float angle; };

    static constexpr uint32_t kCubeCount       = 1500;
    static constexpr int      kPointLightCount = 4;
    static constexpr float kHalfExtent[] = {16.0f, 9.0f, 8.0f};
    static constexpr float kSize[]       = {32.0f, 18.0f, 16.0f};
    static constexpr float kMinScale     = 0.2f;
    static constexpr float kMaxScale     = 0.45f;
    static constexpr float kMinRotSpeed  = 0.3f;
    static constexpr float kMaxRotSpeed  = 1.5f;
    static constexpr float kFov          = 55.0f;
    static constexpr float kCamDistance  = 28.0f;
    static constexpr float kCamYaw       = 35.0f;
    static constexpr float kCamPitch     = 18.0f;

    DM::Ref<DMGameEngine::Shader>            m_UnlitShader;
    DM::Ref<DMGameEngine::MaterialInstance>  m_SharedMaterial;
    DM::Ref<DMGameEngine::Mesh>              m_CubeMesh;
    std::vector<CubeAnim>                    m_Anims;
};

// ──────────────────────────────────────────────────────────────────
class LitCubesGame : public DMGameEngine::Application
{
public:
    LitCubesGame() : Application(DMGameEngine::WindowProps("DMGameEngine - Lit Cubes (ECS + Lighting)", 1280, 720)) {}
    void OnInitialize() override
    {
        DMGE_CLIENT_INFO("LitCubesGame initialized");
        GetWindow().SetVSync(true);
        PushLayer(std::make_unique<LitCubeScene>());
    }
    void OnUpdate(DMGameEngine::Timestep ts) override
    {
        m_frameCount++; m_elapsedTime += ts;
        if (m_elapsedTime >= 1.0f)
        {
            const float fps = static_cast<float>(m_frameCount) / m_elapsedTime;
            m_frameCount = 0; m_elapsedTime = 0.0f;
        }
    }
    void OnEvent(DMGameEngine::Event& e) override
    {
        DMGameEngine::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<DMGameEngine::KeyPressedEvent>(
            [](DMGameEngine::KeyPressedEvent& e) {
                if (e.GetKeyCode() == DMGameEngine::KeyCode::Escape)
                { DMGE_CLIENT_INFO("Escape pressed - quitting"); DMGameEngine::Application::Get().Quit(); return true; }
                return false;
            });
        Application::OnEvent(e);
    }
    void OnShutdown() override { DMGE_CLIENT_INFO("LitCubesGame shutting down"); }
private:
    int   m_frameCount  = 0;
    float m_elapsedTime = 0.0f;
};

DMGameEngine::Application* DMGameEngine::CreateApplication()
{
    // Stage-B demo switch: DMGE_API=D3D11 (env) or -d3d11 (argv) runs the
    // Direct3D 11 backend; default remains OpenGL.
    const bool useD3D11 = WantDirectXBackend();
    DMGameEngine::Renderer::SetAPI(useD3D11
        ? DMGameEngine::Renderer::API::DirectX
        : DMGameEngine::Renderer::API::OpenGL);

    // ROADMAP 3e: the OpenGL demo runs the deferred path (G-buffer +
    // fullscreen lighting pass). The D3D11 deferred alignment (internal
    // shaders are GLSL embedded in Renderer.cpp) is stage C - D3D11 runs
    // the forward path.
    DMGameEngine::Renderer::SetRenderPath(useD3D11
        ? DMGameEngine::RenderPath::Forward
        : DMGameEngine::RenderPath::Deferred);
    return new LitCubesGame();
}
