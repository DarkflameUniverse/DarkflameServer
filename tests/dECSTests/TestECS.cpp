#include <cstdio>
#include <gtest/gtest.h>
#include "Core.h"
#include <dComponents/Component.h>
#include <eReplicaComponentType.h>

using namespace dECS;

struct TestComponent {
    static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHOICE_BUILD;

    int value;
};

// Test that entity IDs increment correctly
TEST(ECSTest, IncrementEntityIdsSingleThread) {
    auto w = World{};

    auto ea = w.MakeEntity();
    ASSERT_EQ(ea.Id(), 1);

    auto eb = w.MakeEntity();
    ASSERT_EQ(eb.Id(), 2);

    auto ec = w.MakeEntity();
    ASSERT_EQ(ec.Id(), 3);
}

// Test adding and getting components
TEST(ECSTest, MakeOneEntityAndAddComponents) {
    auto w = World{};
    auto e = w.MakeEntity();
    ASSERT_EQ(e.Id(), 1);

    // add component
    auto* const testCompPtr = e.AddComponent<TestComponent>();
    ASSERT_NE(testCompPtr, nullptr);
    ASSERT_EQ(testCompPtr->ComponentType, eReplicaComponentType::CHOICE_BUILD);
    ASSERT_EQ(testCompPtr->value, 0);
    testCompPtr->value = 15;

    // try getting the same component we just added
    auto* const getTestCompPtr = e.GetComponent<TestComponent>();
    ASSERT_NE(getTestCompPtr, nullptr);
    ASSERT_EQ(testCompPtr, getTestCompPtr);
    ASSERT_NE(getTestCompPtr->value, 0);
    ASSERT_EQ(getTestCompPtr->value, 15);
}

// Test world scoping
TEST(ECSTest, WorldScope) {
    auto e = std::optional<dECS::Entity>{};

    {
        auto w = World{};
        e.emplace(w.MakeEntity());
        ASSERT_EQ(e->Id(), 1);

        // add component within scope
        auto* const cPtr = e->AddComponent<TestComponent>();
        ASSERT_NE(cPtr, nullptr);
    }

    // Attempting to access this component should return nullptr
    // now that the world has gone out of scope
    ASSERT_EQ(e->GetComponent<TestComponent>(), nullptr);
}
