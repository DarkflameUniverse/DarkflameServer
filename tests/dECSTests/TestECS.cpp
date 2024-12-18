#include <cstdint>
#include <cstddef>
#include <span>
#include <optional>
#include <gtest/gtest.h>
#include "Core.h"
#include <dComponents/Component.h>
#include <eReplicaComponentType.h>

using namespace dECS;

namespace TestECS {
    using LegacyComponent = ::Component;

    namespace Component {
        using enum eReplicaComponentType;

        void* NULL_PARENT = nullptr;

        struct Legacy : public LegacyComponent {
            static constexpr eReplicaComponentType ComponentType = CHANGLING;

            Legacy() = default;

            void Update(float deltaTime) {
                std::printf("Legacy updated!\n");
            }
        };

        struct Invalid {
            static constexpr eReplicaComponentType ComponentType = INVALID;

            int value;
        };

        struct Destroyable {
            static constexpr eReplicaComponentType ComponentType = DESTROYABLE;

            using FactionId = int32_t;

            float health;
            float maxHealth;
            float armor;
            float maxArmor;
            float imag;
            float maxImag;
            uint32_t damageToAbsorb;
            bool immune;
            bool gmImmune;
            bool shielded;
            float actualMaxHealth;
            float actualMaxArmor;
            float actualMaxImagination;
            std::vector<FactionId> factionIds;
            bool smashable;
        };
    }

    struct IFakeSystem {
        virtual ~IFakeSystem() = default;

        [[nodiscard]]
        constexpr virtual size_t Count() const noexcept = 0;

        constexpr virtual void EmplaceBack() = 0;
    };

    template <typename... Cs>
    struct FakeSystem : public IFakeSystem {
        template <typename C>
        using Storage = std::vector<std::remove_const_t<C>>;

        std::tuple<Storage<Cs>...> data;

        [[nodiscard]]
        constexpr size_t Count() const noexcept override {
            return std::get<0>(data).size();
        }

        constexpr void EmplaceBack() override {
            (std::get<Storage<Cs>>(data).emplace_back(), ...);
        }

        template <typename C>
            requires std::disjunction_v<std::is_same<C, Cs>...>
        [[nodiscard]]
        std::span<C> Get() {
            return std::get<Storage<C>>(data);
        }

        template <typename C>
            requires std::disjunction_v<std::is_same<C, Cs>...>
        [[nodiscard]]
        std::span<const C> Get() const {
            return std::get<Storage<C>>(data);
        }

        template <typename Fn>
            requires std::is_invocable_r_v<void, Fn(Cs...), Cs...>
        void ForEach(Fn&& fn);
    };

    class FakeIter {
    public:
        constexpr FakeIter(const IFakeSystem& fakeSys) noexcept
            : m_System{ fakeSys }
        {}

        [[nodiscard]]
        constexpr bool Next() {
            return m_Count++ > m_System.Count();
        }

    private:
        size_t m_Count;
        const IFakeSystem& m_System;
    };

    template <typename... Cs>
    template <typename Fn>
        requires std::is_invocable_r_v<void, Fn(Cs...), Cs...>
    void FakeSystem<Cs...>::ForEach(Fn&& fn){
        for (size_t i = 0; i < Count(); ++i) {
            fn(Get<Cs>()[i]...);
        }
    }
}

// Test that entity IDs increment correctly
TEST(ECSTest, IncrementEntityIdsSingleThread) {
    auto w = World{};

    auto ea = w.MakeEntity();
    ASSERT_EQ(ea.GetObjectID(), 1);

    auto eb = w.MakeEntity();
    ASSERT_EQ(eb.GetObjectID(), 2);

    auto ec = w.MakeEntity();
    ASSERT_EQ(ec.GetObjectID(), 3);
}

// Test adding and getting components
TEST(ECSTest, MakeOneEntityAndAddComponents) {
    using namespace TestECS::Component;

    auto w = World{};
    auto e = w.MakeEntity();
    ASSERT_EQ(e.GetObjectID(), 1);

    // add component
    auto* const testCompPtr = e.AddComponent<Invalid>();
    ASSERT_NE(testCompPtr, nullptr);
    ASSERT_EQ(testCompPtr->ComponentType, Invalid::ComponentType);
    ASSERT_EQ(testCompPtr->value, 0);
    testCompPtr->value = 15;

    // try getting the same component we just added
    auto* const gotTestCompPtr = e.GetComponent<Invalid>();
    ASSERT_NE(gotTestCompPtr, nullptr);
    ASSERT_EQ(gotTestCompPtr, testCompPtr);
    ASSERT_NE(gotTestCompPtr->value, 0);
    ASSERT_EQ(gotTestCompPtr->value, 15);
}

// Test world scoping
TEST(ECSTest, WorldScope) {
    using namespace TestECS::Component;

    auto e = std::optional<dECS::Entity>{};

    {
        auto w = World{};
        e.emplace(w.MakeEntity());
        ASSERT_EQ(e->GetObjectID(), 1);

        // add component within scope
        auto* const cPtr = e->AddComponent<Invalid>();
        ASSERT_NE(cPtr, nullptr);
    }

    // Attempting to access this component should return nullptr
    // now that the world has gone out of scope
    ASSERT_EQ(e->GetComponent<Invalid>(), nullptr);
}

// Create and iterate over a system
TEST(ECSTest, CreateAndIterateOverSystem) {
    using namespace TestECS::Component;

    auto w = World{};
    auto s = w.MakeSystem<Destroyable, const Invalid>("DestInvalid");

    size_t count = 0;
    s.ForEach([&](Destroyable& d, const Invalid& i) {
        std::printf("i = %ld: d.health = %f\n", ++count, d.health);
        d.health += 1;
    });
}

TEST(ECSTest, FakeIterationForTestingPurposes) {
    using namespace TestECS;
    using namespace TestECS::Component;

    auto s = FakeSystem<Legacy, Destroyable>{};

    auto const r = 2 + std::rand() % 8;
    for (size_t i = 0; i < r; ++i) {
        s.EmplaceBack();
    }

    size_t count = 0;
    s.ForEach([&](Legacy& l, Destroyable& d) {
        l.Update(0.0f);
        std::printf("i = %ld: d.health = %f\n", ++count, d.health);
        d.health += 1;
    });
    std::printf("Total count = %ld\n", count);
    ASSERT_EQ(r, count);
}
