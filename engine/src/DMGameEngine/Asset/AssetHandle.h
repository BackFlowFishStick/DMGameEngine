/*
 * DMGameEngine - AssetHandle (stage 1a UUID version)
 *
 * Pure identity token: stores only the AssetUUID, NOT the path. The path is
 * looked up via AssetManager::GetMetadata(uuid).Path when loading. This decouples
 * the serialized reference (UUID, stable) from the resource location (path,
 * mutable) - moving a file updates metadata only, references stay valid.
 *
 * See ASSET_UUID_CONCEPTS.md part 4.
 */
#pragma once
#include "DMGameEngine/Asset/AssetTypes.h"

namespace DMGameEngine {

class AssetHandle
{
    AssetUUID m_UUID = NullUUID;

public:
    AssetHandle() = default;
    explicit AssetHandle(AssetUUID uuid) : m_UUID(uuid) {}

    AssetUUID GetUUID() const { return m_UUID; }
    bool IsValid() const { return m_UUID != NullUUID; }
    explicit operator bool() const { return IsValid(); }

    bool operator==(const AssetHandle&) const = default;

private:
    // Convenience for AssetManager - resolve the path via the registry.
    // (No path stored here; AssetManager holds UUID -> metadata.)
};

} // namespace DMGameEngine