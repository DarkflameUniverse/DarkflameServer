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
#include "BrickByBrick.h"
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

		// Picking something new to build or arrange (a brick for a new model, a placed model to take apart) starts
		// arranging again; the build area is the subject, as for StartBuildingWithItem. Leaving (newSourceType 0) gets
		// no answer, as on live.
		if (PropertyManagementComponent::Instance() != nullptr && newSourceType != 0) {
			StartArrangingWithItem arranging;
			arranging.target = character->GetObjectID();
			arranging.firstTime = false;
			arranging.buildAreaID = entity.GetObjectID();
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
		confirmed.warnVisitors = !start; // as a live server answered: false going in, true coming out
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
		BrickByBrick::ReturnModel(entity, modelID, bHasWorldTransform, worldPos, worldRot);
	}

	void BBBLoadItemRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemID);
	}

	bool BBBLoadItemRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemID));
		return true;
	}

	void BBBLoadItemRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto movedId = BrickByBrick::LoadModel(entity, itemID);

		// A live server answered with the same id: the item keeps it in MODELS_IN_BBB. Without the model the client
		// shows BBB_ERROR_LOADING_BLUEPRINT and drops the load (0x00b75bb0).
		ClientPackets::BlueprintLoadItemResponse response;
		response.success = movedId != LWOOBJID_EMPTY;
		response.itemId = itemID;
		response.destItemId = movedId != LWOOBJID_EMPTY ? movedId : itemID;
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
		BrickByBrick::Save(entity, localID, lxfmlDataCompressed);
	}

	void ActivateBrickMode::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, buildObjectID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, buildType, 2);
		bitStream.Write(enterBuildFromWorld);
		bitStream.Write(enterFlag);
	}

	bool ActivateBrickMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, buildObjectID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, buildType, 2));
		VALIDATE_READ(bitStream.Read(enterBuildFromWorld));
		VALIDATE_READ(bitStream.Read(enterFlag));
		return true;
	}

	void ActivateBrickMode::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!enterFlag) BrickByBrick::EndSession(entity);
	}

	void SetBBBAutosave::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(lxfmlDataCompressed.size());
		if (!lxfmlDataCompressed.empty()) bitStream.Write(lxfmlDataCompressed.data(), lxfmlDataCompressed.size());
	}

	bool SetBBBAutosave::Deserialize(RakNet::BitStream& bitStream) {
		uint32_t size{};
		VALIDATE_READ(bitStream.Read(size));
		if (static_cast<uint64_t>(size) * 8 > bitStream.GetNumberOfUnreadBits()) return false;
		lxfmlDataCompressed.resize(size);
		if (size != 0) VALIDATE_READ(bitStream.Read(lxfmlDataCompressed.data(), size));
		return true;
	}

	void SetBBBAutosave::Handle(Entity& entity, const SystemAddress& sysAddr) {
		BrickByBrick::Autosave(entity, lxfmlDataCompressed);
	}

	void RebuildBBBAutosaveMsg::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(count);
	}

	bool RebuildBBBAutosaveMsg::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(count));
		return true;
	}

	void SetModelToBuild::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelLot != -1);
		if (modelLot != -1) bitStream.Write(modelLot);
	}

	void SpawnModelBricks::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(amount != 0.0f);
		if (amount != 0.0f) bitStream.Write(amount);
		bitStream.Write(position != NiPoint3Constant::ZERO);
		if (position != NiPoint3Constant::ZERO) {
			bitStream.Write(position.x);
			bitStream.Write(position.y);
			bitStream.Write(position.z);
		}
	}

	bool SetModelToBuild::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadOptional(bitStream, modelLot, LOT{ -1 });
	}

	bool SpawnModelBricks::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, amount, 0.0f));
		bool hasPosition{};
		VALIDATE_READ(bitStream.Read(hasPosition));
		position = NiPoint3Constant::ZERO;
		if (hasPosition) {
			VALIDATE_READ(bitStream.Read(position.x));
			VALIDATE_READ(bitStream.Read(position.y));
			VALIDATE_READ(bitStream.Read(position.z));
		}
		return true;
	}
}
