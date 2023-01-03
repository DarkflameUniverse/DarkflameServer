#include "PlayerForcedMovementComponent.h"

PlayerForcedMovementComponent::PlayerForcedMovementComponent(Entity* parent) : Component(parent) {
	m_Parent = parent;
}

PlayerForcedMovementComponent::~PlayerForcedMovementComponent() {}

void PlayerForcedMovementComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyInfo || bIsInitialUpdate);
	if (m_DirtyInfo || bIsInitialUpdate) {
		outBitStream->Write(m_PlayerOnRail);
		outBitStream->Write(m_ShowBillboard);
	}
	m_DirtyInfo = false;
}
