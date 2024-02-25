#include "ItemComponent.h"

void ItemComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write0();
}
