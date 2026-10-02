#include "PlayerForcedMovementComponent.h"
#include "Entity.h"
#include "ObjectLoadState.h"

PlayerForcedMovementComponent::PlayerForcedMovementComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_Parent = parent;
}

PlayerForcedMovementComponent::~PlayerForcedMovementComponent() {}

void PlayerForcedMovementComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(m_DirtyInfo || bIsInitialUpdate);
	if (m_DirtyInfo || bIsInitialUpdate) {
		outBitStream.Write(m_PlayerOnRail);
		outBitStream.Write(m_ShowBillboard);
	}
	m_DirtyInfo = false;
}

void PlayerForcedMovementComponent::Allocator(Entity& entity, ObjectLoadState& state) {
	// From the registry, as the client makes it (a character got one with its character component)
	const auto id = state.GetComponentID(eReplicaComponentType::PLAYER_FORCED_MOVEMENT, -1);
	if (id > -1 && !entity.HasComponent(eReplicaComponentType::PLAYER_FORCED_MOVEMENT)) entity.AddComponent<PlayerForcedMovementComponent>(id);
}
