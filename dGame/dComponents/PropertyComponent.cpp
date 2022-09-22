#include "PropertyComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"

PropertyComponent::PropertyComponent(Entity* parent) : Component(parent) {
	m_PropertyName = parent->GetVar<std::string>(u"propertyName");
	m_PropertyState = new PropertyState();
}

PropertyComponent::~PropertyComponent() = default;

