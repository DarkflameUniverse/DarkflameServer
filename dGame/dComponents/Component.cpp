#include "Component.h"

Component::Component(const LWOOBJID& parentEntityId) noexcept : m_Parent{ parentEntityId } {}

Component::~Component() {}

Entity* Component::GetParent() const {
	return Game::entityManager->GetEntity(m_Parent); //TEMP
}

void Component::Update(float deltaTime) {}

void Component::OnUse(Entity* originator) {}

void Component::UpdateXml(tinyxml2::XMLDocument* doc) {}

void Component::LoadFromXml(tinyxml2::XMLDocument* doc) {}

void Component::Serialize(RakNet::BitStream* outBitStream, bool isConstruction) {}
