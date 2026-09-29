#include "ItemComponent.h"

void ItemComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	// Live wrote the user-generated-content info on every construction of an object with only an item component:
	// no UGC id, moderation status NoStatus, no description.
	outBitStream.Write(isConstruction);
	if (isConstruction) {
		outBitStream.Write<LWOOBJID>(LWOOBJID_EMPTY); // ug_id
		outBitStream.Write<uint32_t>(0); // ug_moderation_status NoStatus
		outBitStream.Write0(); // ug_description
	}
}
