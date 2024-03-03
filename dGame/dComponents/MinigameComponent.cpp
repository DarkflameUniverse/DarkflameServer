#include "MinigameComponent.h"

void MinigameComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write<uint32_t>(0x40000000);
}
