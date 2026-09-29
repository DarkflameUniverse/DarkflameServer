#ifndef __COLLECTIBLECOMPONENT__H__
#define __COLLECTIBLECOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class CollectibleComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::COLLECTIBLE;
	CollectibleComponent(Entity* parentEntity, const int32_t componentID, const int32_t collectibleId);

	int16_t GetCollectibleId() const { return m_CollectibleId; }
	void Serialize(RakNet::BitStream& outBitStream, bool isConstruction) override;

	bool MsgGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo);

	/**
	 * Whether a player's collecting it counts. The server-only CollectibleComponent.requirement_mission names the
	 * mission the collectible belongs to: when that is a mission to accept (Missions.isMission, not an achievement,
	 * which is always open), the collectible only counts while the player has it accepted and not handed in.
	 */
	bool CountsFor(const Entity& player) const;

	int32_t GetRequirementMission() const { return m_RequirementMission; }
private:
	int16_t m_CollectibleId = 0;

	// CollectibleComponent.requirement_mission, -1 for none
	int32_t m_RequirementMission = -1;
};

#endif  //!__COLLECTIBLECOMPONENT__H__
