#include "PlayerForcedMovementComponent.h"

#include "BitStream.h"

void PlayerForcedMovementComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyInfo || bIsInitialUpdate);
	if (m_DirtyInfo || bIsInitialUpdate) {
		outBitStream->Write(m_PlayerOnRail);
		outBitStream->Write(m_ShowBillboard);
		if (!bIsInitialUpdate) m_DirtyInfo = false;
	}
}
