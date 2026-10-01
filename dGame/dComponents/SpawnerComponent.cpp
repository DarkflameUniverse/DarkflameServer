#include "SpawnerComponent.h"

#include "Entity.h"
#include "Game.h"
#include "Spawner.h"
#include "dZoneManager.h"

SpawnerComponent::SpawnerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

// The zone manager stops indexing the spawner before it goes with the object
SpawnerComponent::~SpawnerComponent() {
	if (m_Spawner && Game::zoneManager) Game::zoneManager->ForgetSpawner(m_Parent->GetObjectID(), m_Spawner.get());
}

void SpawnerComponent::SetSpawner(std::unique_ptr<Spawner> spawner) {
	m_Spawner = std::move(spawner);
}
