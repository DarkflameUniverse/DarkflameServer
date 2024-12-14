#pragma once
#include <bitset>
#include <climits>
#include <cstdint>
#include <vector>
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

namespace Component {
    constexpr size_t MAX_KINDS { 32 };

    enum class Kind : uint32_t {
        NONE = 0,
        DESTROYABLE = 7,
        PET = 26,
    };

    using Signature = std::bitset<MAX_KINDS>;

    // Components
    struct Destroyable;
    struct Pet {
        static constexpr Kind KIND_ID = Kind::PET;

        // The ID under which this pet is stored in the database (if it's tamed)
        LWOOBJID m_DatabaseId;

        // The ID of the item from which this pet was creat
        LWOOBJID m_ItemId;

        // The name of this pet
        std::string name;
    };

    class IStorage {
    public:
        constexpr IStorage(const Kind kind) : m_Kind{ kind } {}
        virtual ~IStorage() = default;
        constexpr virtual void Remove(const size_t index) = 0;

        [[nodiscard]]
        constexpr Kind GetKind() const {
            return m_Kind;
        }

    protected:
        Kind m_Kind;
    };

    template <typename C>
    class Storage : public IStorage {
    public:
        constexpr Storage() : IStorage{ C::KIND_ID } {}

        constexpr void Remove(const size_t index) override {
            auto& elementToDelete = m_Vec.at(index);
            auto& lastElement = m_Vec.back();
            std::swap(elementToDelete, lastElement);
            m_Vec.pop_back();
        }

    private:
        std::vector<C> m_Vec;
    };
}
