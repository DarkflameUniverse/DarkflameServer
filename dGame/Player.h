#pragma once

#include "Entity.h"

/**
 * Extended Entity for player data and behavior.
 *
 * Contains properties only a player entity would require, like associated SystemAddress and User.
 *
 * Keeps track of which entities are observed by this user for ghosting.
 */
class Player final : public Entity
{
public:
	explicit Player(const LWOOBJID& objectID, EntityInfo info, User* user, Entity* parentEntity = nullptr);

	/**
	 * Getters
	 */

	User* GetParentUser() const override { return m_ParentUser; };

	const SystemAddress& GetSystemAddress() const override { return m_SystemAddress; };

	const NiPoint3& GetRespawnPosition() const override { return m_respawnPos; };

	const NiQuaternion& GetRespawnRotation() const override { return m_respawnRot; };

	const NiPoint3& GetGhostReferencePoint() const { return m_GhostOverride ? m_GhostOverridePoint : m_GhostReferencePoint; };

	const NiPoint3& GetOriginGhostReferencePoint() const { return m_GhostReferencePoint; };

	const NiPoint3& GetGhostOverridePoint() const { return m_GhostOverridePoint; };

	bool GetGhostOverride() const { return m_GhostOverride; };

	std::map<LWOOBJID, Loot::Info>& GetDroppedLoot() { return m_DroppedLoot; };

	uint64_t GetDroppedCoins() const { return m_DroppedCoins; };

	/**
	 * Setters
	 */

	void SetGhostOverride(bool value) { m_GhostOverride = value; };

	void SetDroppedCoins(const uint64_t value) { m_DroppedCoins = value; };

	void SetSystemAddress(const SystemAddress& value) override;

	void SetRespawnPos(const NiPoint3& position) override;

	void SetRespawnRot(const NiQuaternion& rotation) override;

	void SetGhostReferencePoint(const NiPoint3& value);

	void SetGhostOverridePoint(const NiPoint3& value);

	/**
	 * Ghosting
	 */

	void AddLimboConstruction(LWOOBJID objectId);

	void RemoveLimboConstruction(LWOOBJID objectId);

	void ConstructLimboEntities();

	void ObserveEntity(const int32_t id);

	bool IsObserved(const int32_t id);

	void GhostEntity(const int32_t id);

	~Player() override;
private:
	SystemAddress m_SystemAddress;

	NiPoint3 m_respawnPos;

	NiQuaternion m_respawnRot;

	User* m_ParentUser;

	NiPoint3 m_GhostReferencePoint;

	NiPoint3 m_GhostOverridePoint;

	bool m_GhostOverride;

	std::vector<int32_t> m_ObservedEntities;

	std::vector<LWOOBJID> m_LimboConstructions;

	std::map<LWOOBJID, Loot::Info> m_DroppedLoot;

	uint64_t m_DroppedCoins;
};
