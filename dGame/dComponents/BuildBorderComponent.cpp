#include "BuildBorderComponent.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Game.h"
#include "dLogger.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "PropertyManagementComponent.h"

BuildBorderComponent::BuildBorderComponent(Entity* parent) : Component(parent) {
}

BuildBorderComponent::~BuildBorderComponent() {
}

void BuildBorderComponent::OnUse(Entity* originator) {
	if (originator->GetCharacter()) {
		const auto& entities = EntityManager::Instance()->GetEntitiesInGroup("PropertyPlaque");

		auto buildArea = m_Parent->GetObjectID();

		if (!entities.empty()) {
			buildArea = entities[0]->GetObjectID();

			Game::logger->Log("BuildBorderComponent", "Using PropertyPlaque");
		}

		auto* inventoryComponent = originator->GetComponent<InventoryComponent>();

		if (inventoryComponent == nullptr) {
			return;
		}

		auto* thinkingHat = inventoryComponent->FindItemByLot(6086);

		if (thinkingHat == nullptr) {
			return;
		}

		inventoryComponent->PushEquippedItems();

		Game::logger->Log("BuildBorderComponent", "Starting with %llu", buildArea);

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
				NiPoint3::ZERO,
				0
			);
		} else {
			GameMessages::SendStartArrangingWithItem(originator, originator->GetSystemAddress(), true, buildArea, originator->GetPosition());
		}

		InventoryComponent* inv = m_Parent->GetComponent<InventoryComponent>();
		if (!inv) return;
		inv->PushEquippedItems(); // technically this is supposed to happen automatically... but it doesnt? so just keep this here
	}
}
