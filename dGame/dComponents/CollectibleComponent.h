#ifndef __COLLECTIBLECOMPONENT__H__
#define __COLLECTIBLECOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class CollectibleComponent : public Component {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::COLLECTIBLE;
	CollectibleComponent(const LWOOBJID& parentEntityId, int16_t collectibleId) noexcept : Component{ parentEntityId }, m_CollectibleId{ collectibleId } {}

	int16_t GetCollectibleId() const { return m_CollectibleId; }
	void Serialize(RakNet::BitStream* outBitStream, bool isConstruction) override;
private:
	const int16_t m_CollectibleId = 0;
};

#endif  //!__COLLECTIBLECOMPONENT__H__
