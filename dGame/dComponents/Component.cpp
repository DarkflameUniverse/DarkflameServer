#include "Component.h"


Component::Component(Entity* parent) {
	m_Parent = parent;
}

Component::~Component() {

}

Entity* Component::GetParent() const {
	return m_Parent;
}

void Component::Update(float deltaTime) {

}

void Component::OnUse(Entity* originator) {

}

void Component::UpdateXml(tinyxml2::XMLDocument* doc) {

}

void Component::LoadFromXml(tinyxml2::XMLDocument* doc) {

}
