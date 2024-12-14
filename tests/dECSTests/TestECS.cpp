#include <gtest/gtest.h>
#include "ECS.h"

TEST(ECSTest, MakeStorage) {
    const auto storage = Component::Storage<Component::Pet>();
    const Component::IStorage* const storagePtr = &storage;

    ASSERT_EQ(storagePtr->GetKind(), Component::Kind::PET);
    ASSERT_NE(storagePtr->GetKind(), Component::Kind::DESTROYABLE);
}
