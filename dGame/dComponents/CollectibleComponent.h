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

	bool MsgGetObjectReportInfo(GameMessages::GameMsg& msg);
private:
	int16_t m_CollectibleId = 0;
};

#endif  //!__COLLECTIBLECOMPONENT__H__
