#include "CollectibleComponent.h"

void CollectibleComponent::Serialize(RakNet::BitStream* outBitStream, bool isConstruction) {
	outBitStream->Write(GetCollectibleId());
}
