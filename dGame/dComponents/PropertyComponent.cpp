#include "PropertyComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"

PropertyComponent::PropertyComponent(Entity* parentEntity)
	: Component{ parentEntity }
	, m_PropertyName{ parentEntity->GetVar<std::string>(u"propertyName") }
	, m_PropertyState{ std::make_unique<PropertyState>() } {
}
