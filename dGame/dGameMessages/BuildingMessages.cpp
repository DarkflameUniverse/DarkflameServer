#include "BuildingMessages.h"

#include "BitStreamUtils.h"
#include "Character.h"
#include "ClientPackets.h"
#include "CppScripts.h"
#include "DashboardNotify.h"
#include "Database.h"
#include "dConfig.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "eBlueprintSaveResponseType.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "eMissionTaskType.h"
#include "eRacingTaskParam.h"
#include "eReplicaComponentType.h"
#include "Entity.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "Logger.h"
#include "Lxfml.h"
#include "MissionComponent.h"
#include "ObjectIDManager.h"
#include "PropertyManagementComponent.h"
#include "Sd0.h"
#include "ScriptComponent.h"
#include "User.h"
#include "UserManager.h"

#include <fstream>
#include <sstream>

namespace {
	void WriteBuildModeFields(RakNet::BitStream& bitStream, bool start, int32_t distanceType, bool modePaused, int32_t modeValue, LWOOBJID playerId, const NiPoint3& startPos) {
		bitStream.Write(start);
		BitStreamUtils::WriteOptional(bitStream, distanceType, -1);
		bitStream.Write(modePaused);
		BitStreamUtils::WriteOptional(bitStream, modeValue, 1);
		bitStream.Write(playerId);
		BitStreamUtils::WriteOptional(bitStream, startPos, NiPoint3Constant::ZERO);
	}

	bool ReadBuildModeFields(RakNet::BitStream& bitStream, bool& start, int32_t& distanceType, bool& modePaused, int32_t& modeValue, LWOOBJID& playerId, NiPoint3& startPos) {
		VALIDATE_READ(bitStream.Read(start));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, distanceType, -1));
		VALIDATE_READ(bitStream.Read(modePaused));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, modeValue, 1));
		VALIDATE_READ(bitStream.Read(playerId));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, startPos, NiPoint3Constant::ZERO));
		return true;
	}
}

namespace GameMessages {
	void StartBuildingWithItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(firstTime);
		bitStream.Write(success);
		bitStream.Write(sourceBag);
		bitStream.Write(sourceId);
		bitStream.Write(sourceLot);
		bitStream.Write(sourceType);
		bitStream.Write(targetId);
		bitStream.Write(targetLot);
		bitStream.Write(targetPos);
		bitStream.Write(targetType);
	}

	bool StartBuildingWithItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(firstTime));
		VALIDATE_READ(bitStream.Read(success));
		VALIDATE_READ(bitStream.Read(sourceBag));
		VALIDATE_READ(bitStream.Read(sourceId));
		VALIDATE_READ(bitStream.Read(sourceLot));
		VALIDATE_READ(bitStream.Read(sourceType));
		VALIDATE_READ(bitStream.Read(targetId));
		VALIDATE_READ(bitStream.Read(targetLot));
		VALIDATE_READ(bitStream.Read(targetPos));
		VALIDATE_READ(bitStream.Read(targetType));
		return true;
	}

	void StartBuildingWithItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!entity.HasComponent(eReplicaComponentType::PROPERTY_MANAGEMENT)) {
			return;
		}

		auto arrangedSourceType = sourceType;
		if (arrangedSourceType == 1) {
			arrangedSourceType = 4;
		}

		LOG("Handling start building with item (%i): (%d) (%d) (%i) (%llu) (%i) (%i) (%llu) (%i) (%i)", entity.GetLOT(), firstTime, success, sourceBag, sourceId, sourceLot, arrangedSourceType, targetId, targetLot, targetType);

		auto* user = UserManager::Instance()->GetUser(sysAddr);

		auto* player = Game::entityManager->GetEntity(user->GetLoggedInChar());

		StartArrangingWithItem arranging;
		arranging.target = player->GetObjectID();
		arranging.firstTime = firstTime;
		arranging.buildAreaID = entity.GetObjectID();
		arranging.buildStartPos = player->GetPosition();
		arranging.sourceBag = sourceBag;
		arranging.sourceID = sourceId;
		arranging.sourceLot = sourceLot;
		arranging.sourceType = arrangedSourceType;
		arranging.targetID = targetId;
		arranging.targetLot = targetLot;
		arranging.targetPos = targetPos;
		arranging.targetType = targetType;
		arranging.SendToClient(sysAddr);
	}

	void StartArrangingWithItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(firstTime);
		BitStreamUtils::WriteOptional(bitStream, buildAreaID, LWOOBJID_EMPTY);
		bitStream.Write(buildStartPos);
		bitStream.Write(sourceBag);
		bitStream.Write(sourceID);
		bitStream.Write(sourceLot);
		bitStream.Write(sourceType);
		bitStream.Write(targetID);
		bitStream.Write(targetLot);
		bitStream.Write(targetPos);
		bitStream.Write(targetType);
	}

	bool StartArrangingWithItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(firstTime));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, buildAreaID, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(buildStartPos));
		VALIDATE_READ(bitStream.Read(sourceBag));
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(sourceLot));
		VALIDATE_READ(bitStream.Read(sourceType));
		VALIDATE_READ(bitStream.Read(targetID));
		VALIDATE_READ(bitStream.Read(targetLot));
		VALIDATE_READ(bitStream.Read(targetPos));
		VALIDATE_READ(bitStream.Read(targetType));
		return true;
	}

	void FinishArrangingWithItem::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, buildAreaID, LWOOBJID_EMPTY);
		bitStream.Write(newSourceBag);
		bitStream.Write(newSourceID);
		bitStream.Write(newSourceLot);
		bitStream.Write(newSourceType);
		bitStream.Write(newTargetID);
		bitStream.Write(newTargetLot);
		bitStream.Write(newTargetType);
		bitStream.Write(newTargetPos);
		bitStream.Write(oldItemBag);
		bitStream.Write(oldItemID);
		bitStream.Write(oldItemLot);
		bitStream.Write(oldItemType);
	}

	bool FinishArrangingWithItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, buildAreaID, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(newSourceBag));
		VALIDATE_READ(bitStream.Read(newSourceID));
		VALIDATE_READ(bitStream.Read(newSourceLot));
		VALIDATE_READ(bitStream.Read(newSourceType));
		VALIDATE_READ(bitStream.Read(newTargetID));
		VALIDATE_READ(bitStream.Read(newTargetLot));
		VALIDATE_READ(bitStream.Read(newTargetType));
		VALIDATE_READ(bitStream.Read(newTargetPos));
		VALIDATE_READ(bitStream.Read(oldItemBag));
		VALIDATE_READ(bitStream.Read(oldItemID));
		VALIDATE_READ(bitStream.Read(oldItemLot));
		VALIDATE_READ(bitStream.Read(oldItemType));
		return true;
	}

	void DoneArrangingWithItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(newSourceBag);
		bitStream.Write(newSourceID);
		bitStream.Write(newSourceLot);
		bitStream.Write(newSourceType);
		bitStream.Write(newTargetID);
		bitStream.Write(newTargetLot);
		bitStream.Write(newTargetType);
		bitStream.Write(newTargetPosition);
		bitStream.Write(oldItemBag);
		bitStream.Write(oldItemID);
		bitStream.Write(oldItemLot);
		bitStream.Write(oldItemType);
	}

	bool DoneArrangingWithItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(newSourceBag));
		VALIDATE_READ(bitStream.Read(newSourceID));
		VALIDATE_READ(bitStream.Read(newSourceLot));
		VALIDATE_READ(bitStream.Read(newSourceType));
		VALIDATE_READ(bitStream.Read(newTargetID));
		VALIDATE_READ(bitStream.Read(newTargetLot));
		VALIDATE_READ(bitStream.Read(newTargetType));
		VALIDATE_READ(bitStream.Read(newTargetPosition));
		VALIDATE_READ(bitStream.Read(oldItemBag));
		VALIDATE_READ(bitStream.Read(oldItemID));
		VALIDATE_READ(bitStream.Read(oldItemLot));
		VALIDATE_READ(bitStream.Read(oldItemType));
		return true;
	}

	void DoneArrangingWithItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) return;
		Entity* character = Game::entityManager->GetEntity(user->GetLoggedInChar());
		if (!character) return;
		InventoryComponent* inv = static_cast<InventoryComponent*>(character->GetComponent(eReplicaComponentType::INVENTORY));
		if (!inv) return;

		if (PropertyManagementComponent::Instance() != nullptr) {
			const auto& buildAreas = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::BUILD_BORDER);

			const auto& entities = Game::entityManager->GetEntitiesInGroup("PropertyPlaque");

			Entity* buildArea;

			if (!buildAreas.empty()) {
				buildArea = buildAreas[0];
			} else if (!entities.empty()) {
				buildArea = entities[0];

				LOG("Using PropertyPlaque");
			} else {
				LOG("No build area found");

				return;
			}

			LOG("Build area found: %llu", buildArea->GetObjectID());

			StartArrangingWithItem arranging;
			arranging.target = character->GetObjectID();
			arranging.firstTime = false;
			arranging.buildAreaID = buildArea->GetObjectID();
			arranging.buildStartPos = character->GetPosition();
			arranging.sourceBag = newSourceBag;
			arranging.sourceID = newSourceID;
			arranging.sourceLot = newSourceLot;
			arranging.sourceType = newSourceType;
			arranging.targetID = newTargetID;
			arranging.targetLot = newTargetLot;
			arranging.targetPos = newTargetPosition;
			arranging.targetType = newTargetType;
			arranging.SendToClient(character->GetSystemAddress());
		}

		LOG("Done Arranging");

		auto* inventory = inv->GetInventory(TEMP_MODELS);

		std::vector<Item*> items;

		for (const auto& pair : inventory->GetItems()) {
			items.push_back(pair.second);
		}

		for (auto* item : items) {
			inv->MoveItemToInventory(item, eInventoryType::MODELS, item->GetCount(), false, false);
		}
	}

	void ModularBuildFinish::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint8_t>(modules.size());
		for (const auto module : modules) bitStream.Write(module);
	}

	bool ModularBuildFinish::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t count{}; // 3 for rockets, 7 for cars
		VALIDATE_READ(bitStream.Read(count));
		modules.resize(count);
		for (auto& module : modules) VALIDATE_READ(bitStream.Read(module));
		return true;
	}

	void ModularBuildFinish::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) return;
		Entity* character = Game::entityManager->GetEntity(user->GetLoggedInChar());
		if (!character) return;
		InventoryComponent* inv = static_cast<InventoryComponent*>(character->GetComponent(eReplicaComponentType::INVENTORY));
		if (!inv) return;

		const auto count = modules.size(); // 3 for rockets, 7 for cars

		// Ends the modular build on the client
		const auto finishBuilding = [&]() {
			FinishArrangingWithItem finish;
			finish.target = character->GetObjectID();
			finish.buildAreaID = entity.GetObjectID();
			finish.SendToClient(character->GetSystemAddress());

			ModularBuildEnd end; // i dont know if this does anything but DLUv2 did it
			end.target = character->GetObjectID();
			end.SendToClient(character->GetSystemAddress());
		};

		auto* temp = inv->GetInventory(TEMP_MODELS);
		std::vector<LOT> modList;
		auto& oldPartList = character->GetVar<std::string>(u"currentModifiedBuild");
		bool everyPieceSwapped = !oldPartList.empty(); // If the player didn't put a build in initially, then they should not get this achievement.
		if (count >= 3 && count < 8) {
			std::u16string modulesString;

			for (uint32_t k = 0; k < count; k++) {
				const auto mod = modules[k];
				modList.push_back(mod);
				auto modToStr = GeneralUtils::to_u16string(static_cast<uint32_t>(mod));
				modulesString += u"1:" + (modToStr);
				if (k + 1 != count) modulesString += u"+";

				bool hasItem = false;
				if (temp->GetLotCount(mod) > 0) {
					hasItem = inv->RemoveItem(mod, 1, TEMP_MODELS);
				} else {
					hasItem = inv->RemoveItem(mod, 1, eInventoryType::ALL);
				}

				if (!hasItem) {
					LOG("Player (%llu) attempted to finish a modular build without having all the required parts.", character->GetObjectID());
					finishBuilding(); // kick them from modular build
					return;
				}

				// Doing this check for 1 singular mission that needs to know when you've swapped every part out during a car modular build.
				// since all 8129's are the same, skip checking that
				if (mod != 8129) {
					if (oldPartList.find(GeneralUtils::UTF16ToWTF8(modToStr)) != std::string::npos) everyPieceSwapped = false;

				}
			}

			LOG("Build finished");
			finishBuilding(); // kick them from modular build

			LwoNameValue config;
			config.Insert(u"assemblyPartLOTs", modulesString);

			LWOOBJID newID = ObjectIDManager::GetPersistentID();

			if (count == 3) {
				inv->AddItem(6416, 1, eLootSourceType::QUICKBUILD, eInventoryType::MODELS, config, LWOOBJID_EMPTY, true, false, newID);
			} else if (count == 7) {
				inv->AddItem(8092, 1, eLootSourceType::QUICKBUILD, eInventoryType::MODELS, config, LWOOBJID_EMPTY, true, false, newID);
			}

			auto* pCharacter = character->GetCharacter();
			Database::Get()->InsertUgcBuild(GeneralUtils::UTF16ToWTF8(modulesString), newID, pCharacter ? std::optional(character->GetCharacter()->GetID()) : std::nullopt);

			auto* missionComponent = character->GetComponent<MissionComponent>();

			if (entity.GetLOT() != 9980 || Game::server->GetZoneID() != 1200) {
				if (missionComponent != nullptr) {
					missionComponent->Progress(eMissionTaskType::SCRIPT, entity.GetLOT(), entity.GetObjectID());
					if (count >= 7 && everyPieceSwapped) missionComponent->Progress(eMissionTaskType::RACING, LWOOBJID_EMPTY, static_cast<LWOOBJID>(eRacingTaskParam::MODULAR_BUILDING));
				}
			}

			entity.GetScript()->OnModularBuildExit(&entity, character, count >= 3, modList);

			// Move remaining temp models back to models
			std::vector<Item*> items;

			for (const auto& pair : temp->GetItems()) {
				items.push_back(pair.second);
			}

			for (auto* item : items) {
				inv->MoveItemToInventory(item, eInventoryType::MODELS, item->GetCount(), false);
			}
		}
	}

	void ModularBuildMoveAndEquip::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(templateID);
	}

	bool ModularBuildMoveAndEquip::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(templateID));
		return true;
	}

	void ModularBuildMoveAndEquip::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) return;
		Entity* character = Game::entityManager->GetEntity(user->GetLoggedInChar());
		if (!character) return;

		LOG("Build and move");

		InventoryComponent* inv = static_cast<InventoryComponent*>(character->GetComponent(eReplicaComponentType::INVENTORY));
		if (!inv) return;

		auto* item = inv->FindItemByLot(templateID, TEMP_MODELS);

		if (item == nullptr) {
			return;
		}

		inv->MoveItemToInventory(item, eInventoryType::MODELS, 1, false, true);
	}

	void ModularBuildConvertModel::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelID);
	}

	bool ModularBuildConvertModel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(modelID));
		return true;
	}

	void ModularBuildConvertModel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) return;
		Entity* character = Game::entityManager->GetEntity(user->GetLoggedInChar());
		if (!character) return;
		InventoryComponent* inv = static_cast<InventoryComponent*>(character->GetComponent(eReplicaComponentType::INVENTORY));
		if (!inv) return;

		auto* item = inv->FindItemById(modelID);

		if (item == nullptr) {
			return;
		}

		item->Disassemble(TEMP_MODELS);

		Database::Get()->DeleteUgcBuild(item->GetSubKey());

		item->SetCount(item->GetCount() - 1, false, false, true, eLootSourceType::QUICKBUILD);
	}

	void SetBuildMode::Serialize(RakNet::BitStream& bitStream) const {
		WriteBuildModeFields(bitStream, start, distanceType, modePaused, modeValue, playerId, startPos);
	}

	bool SetBuildMode::Deserialize(RakNet::BitStream& bitStream) {
		return ReadBuildModeFields(bitStream, start, distanceType, modePaused, modeValue, playerId, startPos);
	}

	void SetBuildMode::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = Game::entityManager->GetEntity(playerId);

		auto confirmedStartPos = startPos;
		if (confirmedStartPos == NiPoint3Constant::ZERO) {
			confirmedStartPos = player->GetPosition();
		}

		player->GetCharacter()->SetBuildMode(start);

		LOG("Sending build mode confirm (%i): (%d) (%i) (%d) (%i) (%llu)", entity.GetLOT(), start, distanceType, modePaused, modeValue, playerId);

		SetBuildModeConfirmed confirmed;
		confirmed.target = entity.GetObjectID();
		confirmed.start = start;
		confirmed.warnVisitors = false;
		confirmed.modePaused = modePaused;
		confirmed.modeValue = modeValue;
		confirmed.playerId = playerId;
		confirmed.startPos = confirmedStartPos;
		confirmed.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	void SetBuildModeConfirmed::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(start);
		bitStream.Write(warnVisitors);
		bitStream.Write(modePaused);
		bitStream.Write1(); // modeValue's flag, always set
		bitStream.Write(modeValue);
		bitStream.Write(playerId);
		bitStream.Write1(); // startPos's flag, always set
		bitStream.Write(startPos);
	}

	bool SetBuildModeConfirmed::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(start));
		VALIDATE_READ(bitStream.Read(warnVisitors));
		VALIDATE_READ(bitStream.Read(modePaused));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, modeValue, 1));
		VALIDATE_READ(bitStream.Read(playerId));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, startPos, NiPoint3Constant::ZERO));
		return true;
	}

	void BuildModeSet::Serialize(RakNet::BitStream& bitStream) const {
		WriteBuildModeFields(bitStream, start, distanceType, modePaused, modeValue, playerId, startPos);
	}

	bool BuildModeSet::Deserialize(RakNet::BitStream& bitStream) {
		return ReadBuildModeFields(bitStream, start, distanceType, modePaused, modeValue, playerId, startPos);
	}

	void BuildModeSet::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("Set build mode to (%d) for (%llu)", start, entity.GetObjectID());

		if (entity.GetCharacter()) {
			entity.GetCharacter()->SetBuildMode(start);
		}
	}

	void UnUseBBBModel::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bHasWorldTransform);
		bitStream.Write(modelID);
		BitStreamUtils::WriteOptional(bitStream, worldPos, NiPoint3Constant::ZERO);
		BitStreamUtils::WriteOptional(bitStream, worldRot, QuatUtils::IDENTITY);
	}

	bool UnUseBBBModel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bHasWorldTransform));
		VALIDATE_READ(bitStream.Read(modelID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, worldPos, NiPoint3Constant::ZERO));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, worldRot, QuatUtils::IDENTITY));
		return true;
	}

	void UnUseBBBModel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			auto* inventory = inventoryComponent->GetInventory(eInventoryType::MODELS_IN_BBB);
			auto* item = inventory->FindItemById(modelID);
			if (item) {
				inventoryComponent->MoveItemToInventory(item, eInventoryType::MODELS, 1);
			} else {
				LOG("item id %llu not found in MODELS_IN_BBB inventory, likely because it does not exist", modelID);
			}
		}

		if (bHasWorldTransform) {
			ClientPackets::BlueprintSaveResponse response;
			response.localId = LWOOBJID_EMPTY; //always zero so that a check on the client passes
			response.reasonCode = eBlueprintSaveResponseType::PlacementFailed; // Sending a non-zero error code here prevents the client from deleting its in progress build for some reason?
			response.Send(sysAddr);
		}
	}

	void BBBLoadItemRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemID);
	}

	bool BBBLoadItemRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemID));
		return true;
	}

	void BBBLoadItemRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const LWOOBJID previousItemID = itemID;

		LOG("Load item request for: %lld", previousItemID);
		LWOOBJID newId = previousItemID;
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			auto* inventory = inventoryComponent->GetInventory(eInventoryType::MODELS);
			auto* itemToMove = inventory->FindItemById(previousItemID);

			if (itemToMove) {
				LOT previousLot = itemToMove->GetLot();
				inventoryComponent->MoveItemToInventory(itemToMove, eInventoryType::MODELS_IN_BBB, 1, false);

				auto* destinationInventory = inventoryComponent->GetInventory(eInventoryType::MODELS_IN_BBB);
				if (destinationInventory) {
					auto* movedItem = destinationInventory->FindItemByLot(previousLot);
					if (movedItem) newId = movedItem->GetId();
				}
			} else {
				LOG("item id %llu not found in MODELS inventory, likely because it does not exist", previousItemID);
			}
		}

		// Second argument always true (successful) for now
		ClientPackets::BlueprintLoadItemResponse response;
		response.success = true;
		response.itemId = previousItemID;
		response.destItemId = newId;
		response.Send(sysAddr);
	}

	void BBBSaveRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(localID);
		bitStream.Write<uint32_t>(lxfmlDataCompressed.size());
		bitStream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(lxfmlDataCompressed.data()), lxfmlDataCompressed.size());
		bitStream.Write(timeTakenInMs);
	}

	bool BBBSaveRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(localID));
		uint32_t sd0Size{};
		VALIDATE_READ(bitStream.Read(sd0Size));
		// For the sake of letting players make models as big as they want, the only limit is the message itself.
		bitStream.AlignReadToByteBoundary();
		if (static_cast<uint64_t>(sd0Size) * 8 > bitStream.GetNumberOfUnreadBits()) return false;
		lxfmlDataCompressed.resize(sd0Size);
		if (sd0Size != 0) VALIDATE_READ(bitStream.ReadAlignedBytes(reinterpret_cast<unsigned char*>(lxfmlDataCompressed.data()), sd0Size));
		VALIDATE_READ(bitStream.Read(timeTakenInMs));
		return true;
	}

	void BBBSaveRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		/*
			On DLU we had agreed that bricks wouldn't be taken anyway, but if your server decides otherwise, feel free to
			comment this back out and add the needed code to get the bricks used from lxfml and take them from the inventory.

			Note, in the live client it'll still display the bricks going out as they're being used, but on relog/world change,
			they reappear as we didn't take them.

			TODO Apparently the bricks are supposed to be taken via MoveInventoryBatch?
		*/

		//Now, the cave of dragons:

		//We need to get a new ID for our model first:
		if (!entity.GetCharacter() || !entity.GetCharacter()->GetParentUser()) return;

		//We need to get the propertyID: (stolen from Wincent's propertyManagementComp)
		const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();

		const auto zoneId = worldId.GetMapID();
		const auto cloneId = worldId.GetCloneID();

		auto propertyInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);
		LWOOBJID propertyId = LWOOBJID_EMPTY;
		if (propertyInfo) propertyId = propertyInfo->id;

		// Save the binary data to the Sd0 buffer
		std::istringstream sd0DataStream(lxfmlDataCompressed);
		Sd0 sd0(sd0DataStream);

		// Uncompress the data, split, and nornmalize the model
		const auto asStr = sd0.GetAsStringUncompressed();

		if (Game::config->GetValue("save_lxfmls") == "1") {
			// save using localId to avoid conflicts
			std::ofstream outFile("debug_lxfml_uncompressed_" + std::to_string(localID) + ".lxfml");
			outFile << asStr;
			outFile.close();
		}

		auto splitLxfmls = Lxfml::Split(asStr);
		LOG_DEBUG("Split into %zu models", splitLxfmls.size());

		ClientPackets::BlueprintSaveResponse response;
		response.localId = localID;
		response.reasonCode = eBlueprintSaveResponseType::EverythingWorked;

		std::vector<LWOOBJID> blueprintIDs;
		std::vector<LWOOBJID> modelIDs;

		for (size_t i = 0; i < splitLxfmls.size(); ++i) {
			const auto [newID, blueprintID] = ObjectIDManager::GetNewModelIDs();

			blueprintIDs.push_back(blueprintID);
			modelIDs.push_back(newID);

			// Save each model to the database
			sd0.FromData(reinterpret_cast<const uint8_t*>(splitLxfmls[i].lxfml.data()), splitLxfmls[i].lxfml.size());
			auto sd0AsStream = sd0.GetAsStream();
			Database::Get()->InsertNewUgcModel(sd0AsStream, blueprintID, entity.GetCharacter()->GetParentUser()->GetAccountID(), entity.GetCharacter()->GetID());

			// Insert the new property model
			IPropertyContents::Model model;
			model.id = newID;
			model.ugcId = blueprintID;
			model.position = splitLxfmls[i].center;
			model.rotation = QuatUtils::IDENTITY;
			model.lot = 14;
			Database::Get()->InsertNewPropertyModel(propertyId, model, "Objects_14_name");
			DashboardNotify::Changed("properties", propertyId);

			/*
				Commented out until UGC server would be updated to use a sd0 file instead of lxfml stream.
				(or you uncomment the lxfml decomp stuff above)
			*/

			// Send off to UGC for processing, if enabled:
			// if (Game::config->GetValue("ugc_remote") == "1") {
			// 	std::string ugcIP = Game::config->GetValue("ugc_ip");
			// 	int ugcPort = std::stoi(Game::config->GetValue("ugc_port"));

			// 	httplib::Client cli(ugcIP, ugcPort); //connect to UGC HTTP server using our config above ^

			// 	//Send out a request:
			// 	std::string request = "/3dservices/UGCC150/150" + std::to_string(blueprintID) + ".lxfml";
			// 	cli.Put(request.c_str(), lxfml.c_str(), "text/lxfml");

			// 	//When the "put" above returns, it means that the UGC HTTP server is done processing our model &
			// 	//the nif, hkx and checksum files are ready to be downloaded from cache.
			// }

			// Write the ID and data to the response packet
			auto& responseModel = response.models.emplace_back();
			responseModel.blueprintId = blueprintID;
			for (const auto& chunk : sd0.GetAsVector()) responseModel.data.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
		}

		response.Send(sysAddr);

		// Create entities for each model
		for (size_t i = 0; i < splitLxfmls.size(); ++i) {
			EntityInfo info;
			info.lot = 14;
			info.pos = splitLxfmls[i].center;
			info.rot = QuatUtils::IDENTITY;
			info.spawner = nullptr;
			info.spawnerID = entity.GetObjectID();
			info.spawnerNodeID = 0;

			info.settings.Insert<LWOOBJID>(u"blueprintid", blueprintIDs[i]);
			info.settings.Insert<int>(u"componentWhitelist", 1);
			info.settings.Insert<int>(u"modelType", 2);
			info.settings.Insert<bool>(u"propertyObjectID", true);
			info.settings.Insert<LWOOBJID>(u"userModelID", modelIDs[i]);
			Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);
			if (newEntity) {
				Game::entityManager->ConstructEntity(newEntity);

				//Make sure the propMgmt doesn't delete our model after the server dies
				//Trying to do this after the entity is constructed. Shouldn't really change anything but
				//there was an issue with builds not appearing since it was placed above ConstructEntity.
				PropertyManagementComponent::Instance()->AddModel(newEntity->GetObjectID(), modelIDs[i]);
			}
		}
	}
}
