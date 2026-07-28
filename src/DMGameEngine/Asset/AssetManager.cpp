/*
 * DMGameEngine - AssetManager implementation (stage 1a UUID version)
 */
#include "DMGameEngine/Asset/AssetManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <random>

namespace DMGameEngine {

AssetManager& AssetManager::Get()
{
    static AssetManager instance;
    return instance;
}

AssetUUID AssetManager::GenerateUUID()
{
    // Learning-grade 64-bit random UUID. Production would use 128-bit (e.g. RFC 4122).
    static std::mt19937_64 rng{std::random_device{}()};
    return rng();
}

AssetUUID AssetManager::Register(const std::string& path, AssetType type)
{
    // Already registered by path? Return existing UUID (idempotent).
    if (auto it = m_PathToUUID.find(path); it != m_PathToUUID.end())
        return it->second;

    AssetUUID uuid = GenerateUUID();
    m_Registry[uuid] = AssetMetadata{uuid, path, type, {}};
    m_PathToUUID[path] = uuid;
    return uuid;
}

AssetUUID AssetManager::GetUUID(const std::string& path) const
{
    if (auto it = m_PathToUUID.find(path); it != m_PathToUUID.end())
        return it->second;
    return NullUUID;
}

const AssetMetadata* AssetManager::GetMetadata(AssetUUID uuid) const
{
    if (auto it = m_Registry.find(uuid); it != m_Registry.end())
        return &it->second;
    return nullptr;
}

void AssetManager::Clear()
{
    m_Registry.clear();
    m_PathToUUID.clear();
    m_Cache.clear();
}

void AssetManager::CleanUnused()
{
    for (auto it = m_Cache.begin(); it != m_Cache.end(); )
    {
        if (it->second.expired())
            it = m_Cache.erase(it);
        else
            ++it;
    }
}

bool AssetManager::LoadRegistry(const std::string& registryPath)
{
    std::ifstream fin(registryPath);
    if (!fin.is_open()) return false;

    nlohmann::json j;
    fin >> j;

    for (auto& [uuidStr, meta] : j.items())
    {
        AssetUUID uuid = 0;
        try { uuid = std::stoull(uuidStr); }
        catch (...) { continue; }

        AssetMetadata m;
        m.UUID = uuid;
        m.Path = meta.value("path", "");
        m.Type = static_cast<AssetType>(meta.value("type", 0));
        if (meta.contains("deps"))
            for (auto& d : meta["deps"])
                m.Dependencies.push_back(d.get<AssetUUID>());

        m_Registry[uuid] = std::move(m);
        if (!m_PathToUUID.contains(m.Path))   // first-registered wins on path collision
            m_PathToUUID[m_Registry[uuid].Path] = uuid;
    }
    return true;
}

bool AssetManager::SaveRegistry(const std::string& registryPath) const
{
    nlohmann::json j;
    for (const auto& [uuid, meta] : m_Registry)
    {
        j[std::to_string(uuid)] = {
            {"path", meta.Path},
            {"type", static_cast<int>(meta.Type)},
            {"deps", meta.Dependencies}
        };
    }
    std::ofstream fout(registryPath);
    if (!fout.is_open()) return false;
    fout << j.dump(2);
    return true;
}

} // namespace DMGameEngine