#include "BuildBorderComponent.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Game.h"
#include "Logger.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "PropertyManagementComponent.h"

BuildBorderComponent::BuildBorderComponent(const LWOOBJID& parentEntityId) noexcept : Component{ parentEntityId } {}

void BuildBorderComponent::OnUse(Entity* originator) {
	if (originator->GetCharacter()) {
		const auto& entities = Game::entityManager->GetEntitiesInGroup("PropertyPlaque");

		auto buildArea = m_Parent;

		if (!entities.empty()) {
			buildArea = entities[0]->GetObjectID();

			LOG("Using PropertyPlaque");
		}

		auto* inventoryComponent = originator->GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;

		auto* thinkingHat = inventoryComponent->FindItemByLot(6086);
		if (!thinkingHat) return;

		inventoryComponent->PushEquippedItems();

		LOG("Starting with %llu", buildArea);

		if (PropertyManagementComponent::Instance() != nullptr) {
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
				-1,
				NiPoint3Constant::ZERO,
				0
			);
		} else {
			GameMessages::SendStartArrangingWithItem(originator, originator->GetSystemAddress(), true, buildArea, originator->GetPosition());
		}

		InventoryComponent* inv = Game::entityManager->GetEntity(m_Parent)->GetComponent<InventoryComponent>();
		if (!inv) return;
		inv->PushEquippedItems(); // technically this is supposed to happen automatically... but it doesnt? so just keep this here
	}
}
