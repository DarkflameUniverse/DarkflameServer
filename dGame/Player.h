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

	User* GetParentUser() const override;

	SystemAddress GetSystemAddress() const override;

	NiPoint3 GetRespawnPosition() const override;

	NiQuaternion GetRespawnRotation() const override;

	const NiPoint3& GetGhostReferencePoint() const;

	const NiPoint3& GetOriginGhostReferencePoint() const;

	const NiPoint3& GetGhostOverridePoint() const;

	bool GetGhostOverride() const;

	std::map<LWOOBJID, Loot::Info>& GetDroppedLoot();

	uint64_t GetDroppedCoins();

	/**
	 * Setters
	 */

	void SetSystemAddress(const SystemAddress& value) override;

	void SetRespawnPos(NiPoint3 position) override;

	void SetRespawnRot(NiQuaternion rotation) override;

	void SetGhostReferencePoint(const NiPoint3& value);

	void SetGhostOverridePoint(const NiPoint3& value);

	void SetGhostOverride(bool value);

	void SetDroppedCoins(uint64_t value);

	/**
	 * Wrapper for sending an in-game mail.
	 *
	 * @param sender id of the sender. LWOOBJID_EMPTY for system mail
	 * @param senderName name of the sender. Max 32 characters.
	 * @param subject mail subject. Max 50 characters.
	 * @param body mail body. Max 400 characters.
	 * @param attachment LOT of the attached item. LOT_NULL if no attachment.
	 * @param attachmentCount stack size for attachment.
	 */
	void SendMail(LWOOBJID sender, const std::string& senderName, const std::string& subject, const std::string& body, LOT attachment, uint16_t attachmentCount) const;

	/**
	 * Wrapper for transfering the player to another instance.
	 *
	 * @param zoneId zoneID for the new instance.
	 * @param cloneId cloneID for the new instance.
	 */
	void SendToZone(LWOMAPID zoneId, LWOCLONEID cloneId = 0);

	/**
	 * Ghosting
	 */

	void AddLimboConstruction(LWOOBJID objectId);

	void RemoveLimboConstruction(LWOOBJID objectId);

	void ConstructLimboEntities();

	void ObserveEntity(int32_t id);

	bool IsObserved(int32_t id);

	void GhostEntity(int32_t id);

	/**
	 * Static methods
	 */

	static Player* GetPlayer(const SystemAddress& sysAddr);

	static Player* GetPlayer(const std::string& name);

	static Player* GetPlayer(LWOOBJID playerID);

	static const std::vector<Player*>& GetAllPlayers();

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

	int32_t m_ObservedEntitiesLength;

	int32_t m_ObservedEntitiesUsed;

	std::vector<LWOOBJID> m_LimboConstructions;

	std::map<LWOOBJID, Loot::Info> m_DroppedLoot;

	uint64_t m_DroppedCoins;

	static std::vector<Player*> m_Players;
};
