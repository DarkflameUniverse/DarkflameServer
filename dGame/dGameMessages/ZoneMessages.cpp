#include "ZoneMessages.h"

#include "BitStreamUtils.h"
#include "CDClientManager.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "CppScripts.h"
#include "DestroyableComponent.h"
#include "EffectsMessages.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GhostComponent.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "LevelProgressionComponent.h"
#include "RacingControlComponent.h"
#include "WorldMigration.h"
#include "BrickByBrick.h"
#include "PropertyManagementComponent.h"
#include "dConfig.h"
#include "dZoneManager.h"
#include "eReplicaComponentType.h"
#include "eTriggerEventType.h"
#include "ePlayerFlag.h"

#include <sstream>

namespace GameMessages {
	void PlayerLoaded::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool PlayerLoaded::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(playerID);
	}

	void PlayerLoaded::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		auto* entity = &entityRef;
		PlayerReady playerReady;
		playerReady.target = entity->GetObjectID();
		playerReady.SendToClient(sysAddr);
		entity->SetPlayerReadyForUpdates();

		auto* ghostComponent = entity->GetComponent<GhostComponent>();
		if (ghostComponent != nullptr) {
			ghostComponent->ConstructLimboEntities();
		}

		InventoryComponent* inv = entity->GetComponent<InventoryComponent>();
		if (inv) {
			// Clear server-side skill state so AddItemSkills sends fresh AddSkill
			// packets to the now-ready client. Skills sent during entity construction
			// (Serialize) arrive before LWOSkillComponent is initialized and are dropped.
			inv->ClearSkills();
			auto items = inv->GetEquippedItems();
			for (auto pair : items) {
				const auto item = pair.second;

				inv->AddItemSkills(item.lot);
			}

			// Fixes a bug where testmapping too fast causes large item inventories to become invisible.
			// Only affects item inventory
			GameMessages::SetInventorySize setSize;
			setSize.target = entity->GetObjectID();
			setSize.inventoryType = eInventoryType::ITEMS;
			setSize.size = inv->GetInventory(eInventoryType::ITEMS)->GetSize();
			setSize.SendToClient(entity->GetSystemAddress());
		}

		RestoreToPostLoadStats restoreStats;
		restoreStats.target = entity->GetObjectID();
		restoreStats.SendToClient(sysAddr);

		auto* const destroyable = entity->GetComponent<DestroyableComponent>();
		if (destroyable) destroyable->SetImagination(destroyable->GetImagination());
		Game::entityManager->SerializeEntity(entity);

		std::vector<Entity*> racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
		for (Entity* racingController : racingControllers) {
			auto* racingComponent = racingController->GetComponent<RacingControlComponent>();
			if (racingComponent != nullptr) {
				racingComponent->OnPlayerLoaded(entity);
			}
		}

		Entity* zoneControl = Game::entityManager->GetZoneControlEntity();
		if (zoneControl) {
			zoneControl->GetScript()->OnPlayerLoaded(zoneControl, entity);
		}

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPT);
		for (Entity* scriptEntity : scriptedActs) {
			if (!zoneControl || scriptEntity->GetObjectID() != zoneControl->GetObjectID()) { // Don't want to trigger twice on instance worlds
				scriptEntity->GetScript()->OnPlayerLoaded(scriptEntity, entity);
			}
		}

		//Kill player if health == 0
		if (entity->GetIsDead()) {
			entity->Smash(entity->GetObjectID());
		}

		//if the player has moved significantly, move them back:
		if ((entity->GetPosition().y - entity->GetCharacter()->GetOriginalPos().y) > 2.0f) {
			// Disabled until fixed
			//GameMessages::Teleport(entity->GetObjectID(), entity->GetCharacter()->GetOriginalPos(), entity->GetCharacter()->GetOriginalRot(), true).Send(entity->GetSystemAddress());
		}

		/**
		 * Invoke the OnZoneLoad event on the player character
		 */
		auto* character = entity->GetCharacter();

		if (character != nullptr) {
			character->OnZoneLoad();
		}

		// Moved here from another instance: put back what their save doesn't keep (the pet that was out)
		WorldMigration::OnPlayerLoaded(entity);

		// A brick by brick build that ended without a save (disconnect, crash): rebuild the autosave or give back the
		// models that were open
		BrickByBrick::OnPlayerLoaded(*entity);

		// On a property: whether they are a best friend of the owner, who may build (and the owner arriving lets them)
		if (auto* property = PropertyManagementComponent::Instance()) property->OnPlayerLoaded(*entity);

		LOG("Player %s (%llu) loaded.", entity->GetCharacter()->GetName().c_str(), entity->GetObjectID());

		// After we've done our thing, tell the client they're ready
		PlayerReady zoneReady;
		zoneReady.target = Game::zoneManager->GetZoneControlObject()->GetObjectID();
		zoneReady.SendToClient(sysAddr);

		if (Game::config->GetValue("allow_players_to_skip_cinematics") != "1"
			|| !entity->GetCharacter()
			|| !entity->GetCharacter()->GetPlayerFlag(ePlayerFlag::DLU_SKIP_CINEMATICS)) return;
		entity->AddCallbackTimer(0.5f, [entity, sysAddr]() {
			if (!entity) return;
			GameMessages::EndCinematic endCinematic;
			endCinematic.target = entity->GetObjectID();
			endCinematic.Send(sysAddr);
			});
	}

	void ReadyForUpdates::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
	}

	bool ReadyForUpdates::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(objectID);
	}

	void InvalidZoneTransferList::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, customerFeedbackURL);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, invalidMapTransferList);
		bitStream.Write(bCustomerFeedbackOnExit);
		bitStream.Write(bCustomerFeedbackOnInvalidMapTransfer);
	}

	bool InvalidZoneTransferList::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, customerFeedbackURL));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, invalidMapTransferList));
		VALIDATE_READ(bitStream.Read(bCustomerFeedbackOnExit));
		VALIDATE_READ(bitStream.Read(bCustomerFeedbackOnInvalidMapTransfer));
		return true;
	}

	void DisplayZoneSummary::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(isPropertyMap);
		bitStream.Write(isZoneStart);
		BitStreamUtils::WriteOptional(bitStream, sender, LWOOBJID_EMPTY);
	}

	bool DisplayZoneSummary::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(isPropertyMap));
		VALIDATE_READ(bitStream.Read(isZoneStart));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sender, LWOOBJID_EMPTY));
		return true;
	}

	void ZoneSummaryDismissed::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool ZoneSummaryDismissed::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(playerID);
	}

	void ZoneSummaryDismissed::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = Game::entityManager->GetEntity(playerID);
		entity.TriggerEvent(eTriggerEventType::ZONE_SUMMARY_DISMISSED, player);
	}

	void NotifyServerLevelProcessingComplete::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* levelComp = entity.GetComponent<LevelProgressionComponent>();
		if (!levelComp) return;
		auto* character = entity.GetComponent<CharacterComponent>();
		if (!character) return;

		//Update our character's level in memory:
		levelComp->SetLevel(levelComp->GetLevel() + 1);

		levelComp->HandleLevelUp();

		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();

		if (inventoryComponent != nullptr) {
			auto* inventory = inventoryComponent->GetInventory(ITEMS);

			if (inventory != nullptr && Game::config->GetValue("disable_extra_backpack") != "1") {
				inventory->SetSize(inventory->GetSize() + 2);
			}
		}

		//Play the level up effect:
		GameMessages::PlayFXEffect(entity.GetObjectID(), 7074, u"create", "7074").Send(UNASSIGNED_SYSTEM_ADDRESS);

		//Send a notification in chat:
		std::stringstream wss;
		wss << "level=1:";
		wss << levelComp->GetLevel();
		wss << "\n";
		wss << "name=0:";
		wss << character->GetName();

		// FIXME: only really need utf8 conversion for the name, so move that up?
		std::u16string attrs = GeneralUtils::UTF8ToUTF16(wss.str());
		std::u16string wsText = u"UI_LEVEL_PROGRESSION_LEVELUP_MESSAGE";

		GameMessages::BroadcastTextToChatbox chatboxText;
		chatboxText.target = entity.GetObjectID();
		chatboxText.attrs = attrs;
		chatboxText.wsText = wsText;
		chatboxText.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	void ChangeObjectWorldState::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(newState);
	}

	bool ChangeObjectWorldState::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(newState);
	}

	void LocalizedAnnouncementServerToSingleClient::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteNameValueText(bitStream, bodyParams);
		bitStream.Write(bForceOpenChatBox);
		bitStream.Write(bShowAnnouncement);
		bitStream.Write(bShowInChat);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, body);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, title);
		BitStreamUtils::WriteNameValueText(bitStream, titleParams);
	}

	bool LocalizedAnnouncementServerToSingleClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadNameValueText(bitStream, bodyParams));
		VALIDATE_READ(bitStream.Read(bForceOpenChatBox));
		VALIDATE_READ(bitStream.Read(bShowAnnouncement));
		VALIDATE_READ(bitStream.Read(bShowInChat));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, body));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, title));
		VALIDATE_READ(BitStreamUtils::ReadNameValueText(bitStream, titleParams));
		return true;
	}
}
