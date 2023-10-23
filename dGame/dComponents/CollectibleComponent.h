#ifndef __COLLECTIBLECOMPONENT__H__
#define __COLLECTIBLECOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class CollectibleComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::COLLECTIBLE;
	CollectibleComponent(Entity* parentEntity, int32_t collectibleId) : Component(parentEntity), m_CollectibleId(collectibleId) {}

	int16_t GetCollectibleId() const { return m_CollectibleId; }
	void Serialize(RakNet::BitStream* outBitStream, bool isConstruction) override;
private:
	int16_t m_CollectibleId = 0;
};

#endif  //!__COLLECTIBLECOMPONENT__H__
