#ifndef __ZONECONTROLCOMPONENT__H__
#define __ZONECONTROLCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ZoneControlComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::ZONE_CONTROL;

	ZoneControlComponent(Entity* parent) : Component(parent) {}
	void Serialize(RakNet::BitStream* outBitStream, bool isConstruction);
};

#endif  //!__ZONECONTROLCOMPONENT__H__
