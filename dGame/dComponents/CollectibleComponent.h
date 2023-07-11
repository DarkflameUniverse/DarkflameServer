#ifndef __COLLECTIBLECOMPONENT__H__
#define __COLLECTIBLECOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

#include <cstdint>

class CollectibleComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::COLLECTIBLE;
	CollectibleComponent(Entity* parent) : Component(parent) { };

	void Startup() override;

	uint32_t GetCollectibleId() const { return m_CollectibleId; }
private:
	uint32_t m_CollectibleId;
};


#endif  //!__COLLECTIBLECOMPONENT__H__
