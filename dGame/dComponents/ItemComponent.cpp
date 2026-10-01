#include "ItemComponent.h"

void ItemComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	// Live wrote the user-generated-content info on every construction: the model's userModelID (a brick built model,
	// LOT 14), else no UGC id (items, reward models such as LOT 8069), moderation status NoStatus, no description.
	outBitStream.Write(isConstruction);
	if (isConstruction) {
		outBitStream.Write<LWOOBJID>(m_Parent->GetVarAs<LWOOBJID>(u"userModelID")); // ug_id
		outBitStream.Write<uint32_t>(0); // ug_moderation_status NoStatus
		outBitStream.Write0(); // ug_description
	}
}
