#ifndef BBBCOMPONENT_H
#define BBBCOMPONENT_H

#include "BitStream.h"
#include "Component.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

struct ObjectLoadState;

/**
 * The BBB component (76), as the client's LWOBBBComponent: DoObjectPreLoad gives every character one (and the registry
 * any object listing it). It sends whether there is a metadata source item, then the item's ID.
 */
class BBBComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::BBB;

	// The client's allocator for this type (ObjectLoader2::DoObjectComponentLoad calls it in load order)
	static void Allocator(Entity& entity, ObjectLoadState& state);

	BBBComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

	// LWOBBBComponent::Deserialize 0x00bd6690
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override {
		outBitStream.Write(m_MetadataSourceItem != LWOOBJID_EMPTY);
		if (m_MetadataSourceItem != LWOOBJID_EMPTY) outBitStream.Write(m_MetadataSourceItem);
	}

	void SetMetadataSourceItem(const LWOOBJID item) { m_MetadataSourceItem = item; }

private:
	LWOOBJID m_MetadataSourceItem = LWOOBJID_EMPTY;
};

#endif // BBBCOMPONENT_H
