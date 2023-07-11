#include "CollectibleComponent.h"

#include "Entity.h"

void CollectibleComponent::Startup() {
	m_CollectibleId = GetParentEntity()->GetVarAs<int32_t>(u"collectible_id");
}
