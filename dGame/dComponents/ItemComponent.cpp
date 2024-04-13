#include "ItemComponent.h"

void ItemComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write0();
	/*
	outBitStream.Write(isConstruction || m_Dirty); // Same dirty or different?
	if (isConstruction || m_Dirty) {
		outBitStream.Write(m_parent->GetObjectID());
		outBitStream.Write(moderationStatus);
		outBitStream.Write(!description.empty());
		if (!description.empty()) {
			outBitStream.Write<uint32_t>(description.size());
			outBitStream.Write(description) // u16string
		}
	}
	*/
}
