#include "PropertyComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"

PropertyComponent::PropertyComponent(const LWOOBJID& parentEntityId)
	: Component{ parentEntityId }
	, m_PropertyName{ Game::entityManager->GetEntity(m_Parent)->GetVar<std::string>(u"propertyName") }
	, m_PropertyState{ std::make_unique<PropertyState>() } {
}
