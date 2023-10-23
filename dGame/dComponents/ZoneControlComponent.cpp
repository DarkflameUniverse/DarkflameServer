#include "ZoneControlComponent.h"

void ZoneControlComponent::Serialize(RakNet::BitStream* outBitStream, bool isConstruction) {
	outBitStream->Write<uint32_t>(0x40000000);
}
