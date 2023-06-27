#include "BuildBorderComponent.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Game.h"
#include "dLogger.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "PropertyManagementComponent.h"

void BuildBorderComponent::OnUse(Entity* originator) {
	if (!originator->GetCharacter()) return;

	const auto& entities = EntityManager::Instance()->GetEntitiesInGroup("PropertyPlaque");

	auto buildArea = entities.empty() ? m_ParentEntity->GetObjectID() : entities.front()->GetObjectID();

	auto* inventoryComponent = originator->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return;

	auto* thinkingHat = inventoryComponent->FindItemByLot(LOT_THINKING_CAP);

	if (!thinkingHat) return;

	Game::logger->Log("BuildBorderComponent", "Using BuildArea %llu for player %llu", buildArea, originator->GetObjectID());

	inventoryComponent->PushEquippedItems();

	if (PropertyManagementComponent::Instance()) {
		GameMessages::SendStartArrangingWithItem(
			originator,
			originator->GetSystemAddress(),
			true,
			buildArea,
			originator->GetPosition(),
			0,
			thinkingHat->GetId(),
			thinkingHat->GetLot(),
			4,
			0,
			-1
		);
	} else {
		GameMessages::SendStartArrangingWithItem(originator, originator->GetSystemAddress(), true, buildArea, originator->GetPosition());
	}
}
