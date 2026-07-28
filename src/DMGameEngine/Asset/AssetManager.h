/*
 * DMGameEngine - AssetManager (stage 1a UUID version)
 *
 * Unified resource loading + dedup cache, keyed by AssetUUID (not path).
 * Three tables (see ASSET_UUID_CONCEPTS.md part 2):
 *   m_Registry    : UUID -> AssetMetadata (path, type, dependencies)
 *   m_PathToUUID  : path -> UUID (reverse lookup for path-based Load)
 *   m_Cache       : UUID -> weak_ptr<void> (loaded Ref<T>, auto-release)
 *
 * Load<T>(uuid): cache hit -> return; else resolve path from metadata,
 *   type-check, AssetLoader<T>::Load, cache. Same UUID always returns the
 *   same Ref<T> (dedup). weak_ptr lets refs auto-release when unused.
 *
 * Register(path, type): assign a UUID, record metadata. LoadRegistry /
 *   SaveRegistry persist the UUID<->path mapping across runs so serialized
 *   UUID references stay valid after restart.
 *
 * Minimal: synchronous load. No async/hot-reload/deferred-release (full
 * version scope, see ASSET_DESIGN.md section 10).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Asset/AssetTypes.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/AssetLoader.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace DMGameEngine {

class DMGE_API AssetManager
{
public:
    static AssetManager& Get();

    // ── Load by UUID (main entry, used by serialization) ────────
    template<typename T>
    DM::Ref<T> Load(AssetUUID uuid)
    {
        if (uuid == NullUUID) return nullptr;

        // cache hit?
        if (auto it = m_Cache.find(uuid); it != m_Cache.end())
        {
            if (auto locked = it->second.lock())
                return std::static_pointer_cast<T>(locked);   // cache hit
            m_Cache.erase(it);                                // expired weak_ptr
        }

        // resolve path + type from metadata
        auto mit = m_Registry.find(uuid);
        if (mit == m_Registry.end()) return nullptr;
        const auto& meta = mit->second;
        if (meta.Type != AssetTypeOf<T>()) return nullptr;   // type mismatch

        auto resource = AssetLoader<T>::Load(meta.Path);
        if (resource)
            m_Cache[uuid] = std::weak_ptr<void>(std::static_pointer_cast<void>(resource));
        return resource;
    }

    // Load by AssetHandle (convenience).
    template<typename T>
    DM::Ref<T> Load(const AssetHandle& handle) { return Load<T>(handle.GetUUID()); }

    // Load by path (convenience: resolves path -> UUID, registers if unknown).
    template<typename T>
    DM::Ref<T> Load(const std::string& path)
    {
        AssetUUID uuid = GetUUID(path);
        if (uuid == NullUUID)
            uuid = Register(path, AssetTypeOf<T>());
        return Load<T>(uuid);
    }

    template<typename T>
    bool IsLoaded(AssetUUID uuid) const
    {
        auto it = m_Cache.find(uuid);
        return it != m_Cache.end() && !it->second.expired();
    }

    // ── Registry ────────────────────────────────────────────────
    // Register a resource (assigns UUID, records metadata). If path already
    // registered, returns the existing UUID. Returns NullUUID on failure.
    AssetUUID Register(const std::string& path, AssetType type);

    AssetUUID            GetUUID(const std::string& path) const;
    const AssetMetadata* GetMetadata(AssetUUID uuid) const;

    // Persist UUID<->path mapping across runs (JSON). Returns false on I/O error.
    bool LoadRegistry(const std::string& registryPath);
    bool SaveRegistry(const std::string& registryPath) const;

    // Drop cache entries whose weak_ptr expired.
    void CleanUnused();

private:
    AssetManager() = default;
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    AssetUUID GenerateUUID();

    std::unordered_map<AssetUUID, AssetMetadata>       m_Registry;     // table 1
    std::unordered_map<std::string, AssetUUID>         m_PathToUUID;   // table 2
    std::unordered_map<AssetUUID, std::weak_ptr<void>>  m_Cache;        // table 3
};

} // namespace DMGameEngine