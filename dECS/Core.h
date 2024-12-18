#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

class Component;
enum class eReplicaComponentType : uint32_t;
using LWOOBJID = int64_t;

namespace dECS {
    // template <typename C>
    // concept IsComponent = std::derived_from<C, Component>;

    // Data structures
    struct WorldData;
    class World;

    template <typename... Cs>
    class System;

    class Entity;
    struct IStorage;

    template <typename C>
    class Storage;

    using WorldPtr = std::shared_ptr<WorldData>;
    using WeakWorldPtr = std::weak_ptr<WorldData>;

    class World {
    public:
        World();

        [[nodiscard]]
        Entity MakeEntity();

        template <typename... Cs>
        [[nodiscard]]
        System<Cs...> MakeSystem() {
            return System<Cs...>{};
        }

        template <typename... Cs, typename S>
        [[nodiscard]]
        System<Cs...> MakeSystem(S&& name) {
            return System<Cs...>{ std::forward<S>(name) };
        }

    private:
        WorldPtr m_World;
    };

    template <typename... Cs>
    class System {
    public:
        friend System World::MakeSystem<Cs...>();

        template <typename... Ts, typename S>
        friend System<Ts...> World::MakeSystem(S&&);

        /*template <typename Fn>
            requires std::is_invocable_r_v<void, Fn(Cs...), ObjId, Cs...>
        void ForEach(Fn&& f) {
            for (ObjId i = 0; i < mT.size(); ++i) {
                auto& c = mT[i];
                f(i, std::get<Cs>(c)...);
            }
        }*/

        template <typename Fn>
            requires std::is_invocable_r_v<void, Fn(Cs...), Cs...>
        void ForEach(Fn&& fn) {
            std::tuple<Cs...> comps; // some sort of iterator that returns a tuple each 'step?'
            for (size_t i = 0; i < 5; ++i) {
                fn(std::get<Cs>(comps)...);
            }
        }

    private:
        System() = default;

        template <typename S>
        explicit System(S&& name)
            : m_name{ std::forward<S>(name) }
        {}

        std::string m_name;
    };

    class Entity {
    public:
        friend Entity World::MakeEntity();

        using StorageConstructor = std::function<std::unique_ptr<IStorage>()>;

        [[nodiscard]]
        constexpr LWOOBJID GetObjectID() const noexcept {
            return m_Id;
        }

        [[maybe_unused]]
        void* AddComponent(eReplicaComponentType, StorageConstructor);

        template <typename C>
        [[maybe_unused]]
        C* AddComponent() {
            return static_cast<C*>(AddComponent(C::ComponentType, std::make_unique<Storage<C>>));
        }

        [[nodiscard]]
        const void* GetComponent(eReplicaComponentType) const;

        [[nodiscard]]
        void* GetComponent(eReplicaComponentType);

        template <typename C>
        [[nodiscard]]
        const C* GetComponent() const {
            return static_cast<const C*>(GetComponent(C::ComponentType));
        }

        template <typename C>
        [[nodiscard]]
        C* GetComponent() {
            return static_cast<C*>(GetComponent(C::ComponentType));
        }

    private:
        Entity(const LWOOBJID id, const WeakWorldPtr world)
            : m_Id{ id }
            , m_World { world }
            {}

        LWOOBJID m_Id;

        WeakWorldPtr m_World;
    };

    struct IStorage {
        using RowMap = std::unordered_map<LWOOBJID, size_t>;

        virtual ~IStorage() = default;

        [[nodiscard]]
        virtual void* at(size_t) = 0;

        [[nodiscard]]
        virtual const void* at(size_t) const = 0;

        [[nodiscard]]
        virtual void* emplace_back() = 0;

        RowMap rowMap;
    };

    template <typename C>
    class Storage : public IStorage {
    public:
        [[nodiscard]]
        void* at(const size_t index) override {
            return static_cast<void*>(&m_Vec.at(index));
        }

        [[nodiscard]]
        const void* at(const size_t index) const override {
            return static_cast<const void*>(&m_Vec.at(index));
        }

        [[nodiscard]]
        void* emplace_back() override {
            return static_cast<void*>(&m_Vec.emplace_back());
        }

    private:
        std::vector<C> m_Vec;
    };
}
