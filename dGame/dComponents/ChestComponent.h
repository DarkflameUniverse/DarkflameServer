#ifndef CHESTCOMPONENT_H
#define CHESTCOMPONENT_H

#include "BitStream.h"
#include "Component.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

/**
 * Chest, as the client's LWOChestComponent makes it from the registry (LOT 2196). Its construction carries the player
 * the chest is for and the number of loot offers in it; nothing else of it is done on the server yet.
 */
class ChestComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHEST;

	ChestComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

	// LWOChestComponent::Deserialize (0x00c22010) reads on construction only: the player (u64) and the loot offer count
	// (u32), which it keeps when the chest is the local player's. Nothing in an update.
	void Serialize(RakNet::BitStream& outBitStream, const bool isConstruction) override {
		if (!isConstruction) return;
		outBitStream.Write(m_PlayerID);
		outBitStream.Write(m_LootOfferCount);
	}

	LWOOBJID GetPlayerID() const { return m_PlayerID; }
	void SetPlayerID(const LWOOBJID value) { m_PlayerID = value; }
	uint32_t GetLootOfferCount() const { return m_LootOfferCount; }
	void SetLootOfferCount(const uint32_t value) { m_LootOfferCount = value; }

private:
	// The player the chest belongs to; none (the client's own start value) until something gives it one
	LWOOBJID m_PlayerID = LWOOBJID_EMPTY;

	// The loot offers in the chest
	uint32_t m_LootOfferCount = 0;
};

#endif // CHESTCOMPONENT_H
