#include "PlayerForcedMovementComponent.h"

PlayerForcedMovementComponent::PlayerForcedMovementComponent(const LWOOBJID& parentEntityId) noexcept : Component{ parentEntityId } {}

void PlayerForcedMovementComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(m_DirtyInfo || bIsInitialUpdate);
	if (m_DirtyInfo || bIsInitialUpdate) {
		outBitStream->Write(m_PlayerOnRail);
		outBitStream->Write(m_ShowBillboard);
	}
	m_DirtyInfo = false;
}
