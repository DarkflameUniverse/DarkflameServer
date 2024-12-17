#include <atomic>
#include <magic_enum/magic_enum_containers.hpp>
#include <eReplicaComponentType.h>
#include "Core.h"

namespace dECS {
    struct WorldData {
        using CompSignature = magic_enum::containers::bitset<eReplicaComponentType>;
        using CompMap = std::unordered_map<LWOOBJID, CompSignature>;
        using CompStorage = std::unordered_map<eReplicaComponentType, std::unique_ptr<IStorage>>;

        std::atomic<LWOOBJID> nextId = 1;
        CompMap map;
        CompStorage data;
    };

    World::World() : m_World{ std::make_shared<WorldData>() } {};

    Entity World::MakeEntity() {
        return Entity{ m_World->nextId.fetch_add(1, std::memory_order::relaxed),
                       m_World };
    }

    void* Entity::AddComponent(const eReplicaComponentType kind, const StorageConstructor storageConstructor) {
        if (auto w = m_World.lock()) {
            // Add to kind signature
            w->map[m_Id].set(kind, true);

            // Get or add storage
            auto storageIt = w->data.find(kind);
            if (storageIt == w->data.cend()) {
                bool inserted = false;
                std::tie(storageIt, inserted) = w->data.try_emplace(kind, storageConstructor());
                if (!inserted) throw "storage emplacement failure";
            }
            auto& storage = *storageIt->second;

            // Return reference if already mapped, otherwise add component
            auto compIt = storage.rowMap.find(m_Id);
            if (compIt == storage.rowMap.cend()) {
                const auto curSize = storage.rowMap.size();
                storage.rowMap.emplace(m_Id, curSize);
                return storage.emplace_back();
            }
            const auto row = compIt->second;
            return storage.at(row);
        }
        return nullptr;
    }

    const void* Entity::GetComponent(const eReplicaComponentType kind) const {
        if (auto const w = m_World.lock()) {
            // Check that the entity has this component
            if (!w->map[m_Id].test(kind)) return nullptr;

            // Get the location where it's stored
            const auto& storage = *w->data.at(kind);
            const auto it = storage.rowMap.find(m_Id);
            if (it == storage.rowMap.cend()) return nullptr;
            const auto row = it->second;
            return storage.at(row);
        }
        return nullptr;
    }

    void* Entity::GetComponent(const eReplicaComponentType kind) {
        // Casting away const for this overload is safe, if not at all pretty
        return const_cast<void*>(std::as_const(*this).GetComponent(kind));
    }
}
