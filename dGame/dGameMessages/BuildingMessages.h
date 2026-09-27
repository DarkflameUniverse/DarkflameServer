#ifndef BUILDINGMESSAGES_H
#define BUILDINGMESSAGES_H

#include "GameMessages.h"

#include "NiQuaternion.h"

#include <string>
#include <vector>

// Game messages for building: build mode, arranging models on a property or in a modular build area, modular builds
// (rockets and cars) and brick by brick building.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
namespace GameMessages {
	// Client -> server. The player picked something to build with.
	struct StartBuildingWithItem : public NetGameMsg {
		StartBuildingWithItem() : NetGameMsg(MessageType::Game::START_BUILDING_WITH_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool firstTime{ true };
		bool success{};
		int32_t sourceBag{};
		LWOOBJID sourceId{};
		LOT sourceLot{};
		int32_t sourceType{};
		LWOOBJID targetId{};
		LOT targetLot{};
		NiPoint3 targetPos{};
		int32_t targetType{};
	};

	// Server -> client. Sent to one client only.
	struct StartArrangingWithItem : public NetGameMsg {
		StartArrangingWithItem() : NetGameMsg(MessageType::Game::START_ARRANGING_WITH_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool firstTime{ true };
		LWOOBJID buildAreaID{ LWOOBJID_EMPTY }; // optional
		NiPoint3 buildStartPos{ NiPoint3Constant::ZERO };
		int32_t sourceBag{};
		LWOOBJID sourceID{ LWOOBJID_EMPTY };
		LOT sourceLot{};
		int32_t sourceType{ 8 };
		LWOOBJID targetID{};
		LOT targetLot{};
		NiPoint3 targetPos{ NiPoint3Constant::ZERO };
		int32_t targetType{};
	};

	// Server -> client. Sent to one client only; ends arranging (and a modular build).
	struct FinishArrangingWithItem : public NetGameMsg {
		FinishArrangingWithItem() : NetGameMsg(MessageType::Game::FINISH_ARRANGING_WITH_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID buildAreaID{ LWOOBJID_EMPTY }; // optional
		int32_t newSourceBag{};
		LWOOBJID newSourceID{ LWOOBJID_EMPTY };
		LOT newSourceLot{ LOT_NULL };
		int32_t newSourceType{};
		LWOOBJID newTargetID{ LWOOBJID_EMPTY };
		LOT newTargetLot{ LOT_NULL };
		int32_t newTargetType{};
		NiPoint3 newTargetPos{};
		int32_t oldItemBag{};
		LWOOBJID oldItemID{ LWOOBJID_EMPTY };
		LOT oldItemLot{ LOT_NULL };
		int32_t oldItemType{};
	};

	// Client -> server.
	struct DoneArrangingWithItem : public NetGameMsg {
		DoneArrangingWithItem() : NetGameMsg(MessageType::Game::DONE_ARRANGING_WITH_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t newSourceBag{};
		LWOOBJID newSourceID{};
		LOT newSourceLot{};
		int32_t newSourceType{};
		LWOOBJID newTargetID{};
		LOT newTargetLot{};
		int32_t newTargetType{};
		NiPoint3 newTargetPosition{};
		int32_t oldItemBag{};
		LWOOBJID oldItemID{};
		LOT oldItemLot{};
		int32_t oldItemType{};
	};

	// Server -> client. No payload. Sent to one client only.
	struct ModularBuildEnd : public NetGameMsg {
		ModularBuildEnd() : NetGameMsg(MessageType::Game::MODULAR_BUILD_END) {}
	};

	// Client -> server. The finished modular build: 3 parts for a rocket, 7 for a car.
	struct ModularBuildFinish : public NetGameMsg {
		ModularBuildFinish() : NetGameMsg(MessageType::Game::MODULAR_BUILD_FINISH) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::vector<LOT> modules{}; // u8 count, then the part LOTs
	};

	// Client -> server.
	struct ModularBuildMoveAndEquip : public NetGameMsg {
		ModularBuildMoveAndEquip() : NetGameMsg(MessageType::Game::MODULAR_BUILD_MOVE_AND_EQUIP) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LOT templateID{};
	};

	// Client -> server. Takes a modular build apart again.
	struct ModularBuildConvertModel : public NetGameMsg {
		ModularBuildConvertModel() : NetGameMsg(MessageType::Game::MODULAR_BUILD_CONVERT_MODEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID modelID{};
	};

	// Client -> server.
	struct SetBuildMode : public NetGameMsg {
		SetBuildMode() : NetGameMsg(MessageType::Game::SET_BUILD_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool start{};
		int32_t distanceType{ -1 }; // optional
		bool modePaused{};
		int32_t modeValue{ 1 }; // optional
		LWOOBJID playerId{};
		NiPoint3 startPos{ NiPoint3Constant::ZERO }; // optional
	};

	// Server -> client. modeValue and startPos are optional in the client, but DLU always writes their flag set.
	struct SetBuildModeConfirmed : public NetGameMsg {
		SetBuildModeConfirmed() : NetGameMsg(MessageType::Game::SET_BUILD_MODE_CONFIRMED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool start{};
		bool warnVisitors{ true };
		bool modePaused{};
		int32_t modeValue{ 1 };
		LWOOBJID playerId{};
		NiPoint3 startPos{ NiPoint3Constant::ZERO };
	};

	// Client -> server.
	struct BuildModeSet : public NetGameMsg {
		BuildModeSet() : NetGameMsg(MessageType::Game::BUILD_MODE_SET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool start{};
		int32_t distanceType{ -1 }; // optional
		bool modePaused{};
		int32_t modeValue{ 1 }; // optional
		LWOOBJID playerId{};
		NiPoint3 startPos{ NiPoint3Constant::ZERO }; // optional
	};

	// Client -> server. The player put a model back from brick by brick building.
	struct UnUseBBBModel : public NetGameMsg {
		UnUseBBBModel() : NetGameMsg(MessageType::Game::UN_USE_BBB_MODEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bHasWorldTransform{};
		LWOOBJID modelID{};
		NiPoint3 worldPos{ NiPoint3Constant::ZERO }; // optional
		NiQuaternion worldRot{ QuatUtils::IDENTITY }; // optional
	};

	// Client -> server. The player opened a model in brick by brick building.
	struct BBBLoadItemRequest : public NetGameMsg {
		BBBLoadItemRequest() : NetGameMsg(MessageType::Game::BBB_LOAD_ITEM_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID itemID{};
	};

	// Client -> server. A brick by brick model: its sd0 compressed LXFML.
	struct BBBSaveRequest : public NetGameMsg {
		BBBSaveRequest() : NetGameMsg(MessageType::Game::BBB_SAVE_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID localID{};
		std::string lxfmlDataCompressed{}; // u32 byte count, then the bytes
		uint32_t timeTakenInMs{};
	};

	// Client -> server. The player went into (enterFlag) or came out of brick by brick building.
	// Layout as GameMessage::ActivateBrickMode::Deserialize (0x00d8ecb0) reads it.
	struct ActivateBrickMode : public NetGameMsg {
		ActivateBrickMode() : NetGameMsg(MessageType::Game::ACTIVATE_BRICK_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID buildObjectID{ LWOOBJID_EMPTY }; // optional; the property's build area when entering
		int32_t buildType{ 2 }; // optional; 0 nowhere, 1 in the world, 2 on a property
		bool enterBuildFromWorld{ true };
		bool enterFlag{ true };
	};

	// Client -> server. The client's quick save of the model being built: sd0 compressed LXFML, or just the sd0 header
	// when there is nothing to keep. Sent every five minutes, when told the player is AFK (msgInformAFK) and before the
	// client shuts down. Layout as GameMessage::SetBBBAutosave::Deserialize (0x00f2af60) reads it.
	struct SetBBBAutosave : public NetGameMsg {
		SetBBBAutosave() : NetGameMsg(MessageType::Game::SET_BBB_AUTOSAVE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::string lxfmlDataCompressed{}; // u32 byte count, then the bytes
	};

	// Server -> client. How many unfinished (autosaved) models the server rebuilt into models; the client shows
	// BBB_AUTOSAVE_REBUILDING_SINGLE or _MULTIPLE (LWOBBBComponent_Client::msgRebuildBBBAutosaveMsg, 0x00cfabd0).
	struct RebuildBBBAutosaveMsg : public NetGameMsg {
		RebuildBBBAutosaveMsg() : NetGameMsg(MessageType::Game::REBUILD_BBB_AUTOSAVE_MSG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t count{};
	};

	// A model's user generated data (UGObjectMetadata::Serialize, 0x00f5f590)
	struct UGObjectMetadata {
		LWOOBJID userModelID{};
		LWOOBJID blueprintID{};
		std::u16string userModelName;
		std::u16string userModelDesc;
		LWOOBJID owningPlayerID{};
		LWOOBJID accountID{};
		std::u16string owningPlayerName;
		std::vector<LWOOBJID> userModelBehaviors; // a u8 count on the wire; live always sent 5

		void Serialize(RakNet::BitStream& bitStream) const;
		bool Deserialize(RakNet::BitStream& bitStream);
	};

	// A brick built model's blueprint data (BlueprintMetadata::Serialize, 0x00f5f760)
	struct BlueprintMetadata {
		LWOOBJID blueprintID{};
		int64_t blueprintCreationTimestamp{}; // Unix seconds
		int32_t userModelMod{ 1 };
		NiPoint3 modelBoxMins{};  // the model's box, relative to its origin (the exhibit checks its size)
		NiPoint3 modelBoxMaxs{};
		bool userModelOpt{ true };
		bool ugcIconReady{ true };
		std::u16string brickListColonDelim; // every brick's LOT, each followed by ':'
		bool neverFalseIfPresentInCaps{ true };
		int32_t numberOfBricks{};

		void Serialize(RakNet::BitStream& bitStream) const;
		bool Deserialize(RakNet::BitStream& bitStream);
	};

	// Client -> server. The client wants the name, owner, behaviors and blueprint of a model (its UGID) it has no
	// metadata for: a brick built model item's tooltip (LWOInventoryComponent_Client::FillinDetailsBlueprint, 0x00cddb40, via
	// UGModelMetadataCache::GetModelMetadata, 0x00b5e680), a model on the property, an exhibit.
	struct FetchModelMetadataRequest : public NetGameMsg {
		FetchModelMetadataRequest() : NetGameMsg(MessageType::Game::FETCH_MODEL_METADATA_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t context{};
		LWOOBJID objectID{};
		LWOOBJID requestorID{};
		LWOOBJID ugID{};
	};

	// Server -> client. The answer: the client caches it by UGID and blueprint id and writes it into the config of
	// the inventory item objectID (LWOInventoryComponent_Client::msgFetchModelMetadataResponse, 0x00c64300).
	struct FetchModelMetadataResponse : public NetGameMsg {
		FetchModelMetadataResponse() : NetGameMsg(MessageType::Game::FETCH_MODEL_METADATA_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID ugID{};
		LWOOBJID objectID{};
		LWOOBJID requestorID{};
		int32_t context{};
		bool bHasUGData{};
		bool bHasBPData{};
		UGObjectMetadata ugData;     // when bHasUGData
		BlueprintMetadata bpData;    // when bHasBPData
	};

	struct SetModelToBuild : public NetGameMsg {
		SetModelToBuild() : NetGameMsg(MessageType::Game::SET_MODEL_TO_BUILD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		LOT modelLot{ -1 };
	};

	struct SpawnModelBricks : public NetGameMsg {
		SpawnModelBricks() : NetGameMsg(MessageType::Game::SPAWN_MODEL_BRICKS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float amount{ 0.0f };
		NiPoint3 position{ NiPoint3Constant::ZERO };
	};
};

#endif // BUILDINGMESSAGES_H
