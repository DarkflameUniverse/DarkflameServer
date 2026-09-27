#include "BuildBorderComponent.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "BuildingMessages.h"
#include "Entity.h"
#include "Game.h"
#include "Logger.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "PropertyManagementComponent.h"

BuildBorderComponent::BuildBorderComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
}

BuildBorderComponent::~BuildBorderComponent() {
}

void BuildBorderComponent::OnUse(Entity* originator) {
	if (originator->GetCharacter()) {
		const auto& entities = Game::entityManager->GetEntitiesInGroup("PropertyPlaque");

		auto buildArea = m_Parent->GetObjectID();

		if (!entities.empty()) {
			buildArea = entities[0]->GetObjectID();

			LOG("Using PropertyPlaque");
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

		LOG("Starting with %llu", buildArea);

		if (PropertyManagementComponent::Instance() != nullptr) {
			GameMessages::StartArrangingWithItem arranging;
			arranging.target = originator->GetObjectID();
			arranging.firstTime = true;
			arranging.buildAreaID = buildArea;
			arranging.buildStartPos = originator->GetPosition();
			arranging.sourceBag = 0;
			arranging.sourceID = thinkingHat->GetId();
			arranging.sourceLot = thinkingHat->GetLot();
			arranging.sourceType = 4;
			arranging.targetID = 0;
			arranging.targetLot = -1;
			arranging.targetPos = NiPoint3Constant::ZERO;
			arranging.targetType = 0;
			arranging.SendToClient(originator->GetSystemAddress());
		} else {
			GameMessages::StartArrangingWithItem arranging;
			arranging.target = originator->GetObjectID();
			arranging.firstTime = true;
			arranging.buildAreaID = buildArea;
			arranging.buildStartPos = originator->GetPosition();
			arranging.SendToClient(originator->GetSystemAddress());
		}

		InventoryComponent* inv = m_Parent->GetComponent<InventoryComponent>();
		if (!inv) return;
		inv->PushEquippedItems(); // technically this is supposed to happen automatically... but it doesnt? so just keep this here
	}
}
