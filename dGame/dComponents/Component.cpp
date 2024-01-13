#include "Component.h"

Component::Component(Entity* parent) noexcept : m_Parent{ parent->GetObjectID() } {} //TEMP

Component::~Component() {}

Entity* Component::GetParent() const {
	return Game::entityManager->GetEntity(m_Parent); //TEMP
}

void Component::Update(float deltaTime) {}

void Component::OnUse(Entity* originator) {}

void Component::UpdateXml(tinyxml2::XMLDocument* doc) {}

void Component::LoadFromXml(tinyxml2::XMLDocument* doc) {}

void Component::Serialize(RakNet::BitStream* outBitStream, bool isConstruction) {}
