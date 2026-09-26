#ifndef PETMESSAGES_H
#define PETMESSAGES_H

#include "GameMessages.h"

#include "Brick.h"
#include "ePetAbilityType.h"
#include "ePetTamingNotifyType.h"
#include "NiQuaternion.h"

#include <string>
#include <vector>

// Game messages for pets: the taming minigame, naming, commands and pet bouncers.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// Messages received from a client are handled by the player's taming or active pet (PetComponent).
namespace GameMessages {
	// Server -> client.
	struct NotifyPetTamingMinigame : public NetGameMsg {
		NotifyPetTamingMinigame() : NetGameMsg(MessageType::Game::NOTIFY_PET_TAMING_MINIGAME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID PetID{};
		LWOOBJID PlayerTamingID{};
		bool bForceTeleport{};
		ePetTamingNotifyType notifyType{};
		NiPoint3 petsDestPos{};
		NiPoint3 telePos{};
		NiQuaternion teleRot{ QuatUtils::IDENTITY }; // optional
	};

	// Both directions. The server never sends it; the client sends it when the player leaves the minigame.
	struct ClientExitTamingMinigame : public NetGameMsg {
		ClientExitTamingMinigame() : NetGameMsg(MessageType::Game::CLIENT_EXIT_TAMING_MINIGAME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bVoluntaryExit{ true };
	};

	// Client -> server. No payload.
	struct StartServerPetMinigameTimer : public NetGameMsg {
		StartServerPetMinigameTimer() : NetGameMsg(MessageType::Game::START_SERVER_PET_MINIGAME_TIMER) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client. No payload.
	struct NotifyTamingModelLoadedOnServer : public NetGameMsg {
		NotifyTamingModelLoadedOnServer() : NetGameMsg(MessageType::Game::NOTIFY_TAMING_MODEL_LOADED_ON_SERVER) {}
	};

	// Server -> client. The client's Deserialize (0x00e3a7c0) reads one more u32 after the bricks that its own
	// Serialize (0x00db6880) never writes; DLU writes what Serialize writes.
	struct NotifyPetTamingPuzzleSelected : public NetGameMsg {
		NotifyPetTamingPuzzleSelected() : NetGameMsg(MessageType::Game::NOTIFY_TAMING_PUZZLE_SELECTED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::vector<Brick> bricks{}; // u32 count, then designerID and materialID per brick
	};

	// Client -> server.
	struct PetTamingTryBuild : public NetGameMsg {
		PetTamingTryBuild() : NetGameMsg(MessageType::Game::PET_TAMING_TRY_BUILD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::vector<Brick> bricks{}; // u32 count, then designerID and materialID per brick
		bool clientFailed{};
	};

	// Server -> client.
	struct PetTamingTryBuildResult : public NetGameMsg {
		PetTamingTryBuildResult() : NetGameMsg(MessageType::Game::PET_TAMING_TRY_BUILD_RESULT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bSuccess{ true };
		int32_t iNumCorrect{}; // optional
	};

	// Client -> server.
	struct NotifyTamingBuildSuccess : public NetGameMsg {
		NotifyTamingBuildSuccess() : NetGameMsg(MessageType::Game::NOTIFY_TAMING_BUILD_SUCCESS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		NiPoint3 buildPosition{};
	};

	// Server -> client.
	struct PetResponse : public NetGameMsg {
		PetResponse() : NetGameMsg(MessageType::Game::PET_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID ObjIDPet{};
		int32_t iPetCommandType{};
		int32_t iResponse{};
		int32_t iTypeID{};
	};

	// Server -> client.
	struct AddPetToPlayer : public NetGameMsg {
		AddPetToPlayer() : NetGameMsg(MessageType::Game::ADD_PET_TO_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t iElementalType{};
		std::u16string name{};
		LWOOBJID petDBID{};
		LOT petLOT{};
	};

	// Server -> client.
	struct RegisterPetID : public NetGameMsg {
		RegisterPetID() : NetGameMsg(MessageType::Game::REGISTER_PET_ID) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID objID{};
	};

	// Server -> client.
	struct RegisterPetDBID : public NetGameMsg {
		RegisterPetDBID() : NetGameMsg(MessageType::Game::REGISTER_PET_DBID) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID petDBID{};
	};

	// Server -> client.
	struct ShowPetActionButton : public NetGameMsg {
		ShowPetActionButton() : NetGameMsg(MessageType::Game::SHOW_PET_ACTION_BUTTON) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		ePetAbilityType ButtonLabel{};
		bool bShow{};
	};

	// Server -> client. Turns a bouncer (usually one a pet uncovered) on or off.
	struct BouncerActiveStatus : public NetGameMsg {
		BouncerActiveStatus() : NetGameMsg(MessageType::Game::BOUNCER_ACTIVE_STATUS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bActive{};
	};

	// Client -> server.
	struct RequestSetPetName : public NetGameMsg {
		RequestSetPetName() : NetGameMsg(MessageType::Game::REQUEST_SET_PET_NAME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string name{};
	};

	// Server -> client.
	struct SetPetName : public NetGameMsg {
		SetPetName() : NetGameMsg(MessageType::Game::SET_PET_NAME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string name{};
		LWOOBJID petDBID{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client.
	struct SetPetNameModerated : public NetGameMsg {
		SetPetNameModerated() : NetGameMsg(MessageType::Game::SET_PET_NAME_MODERATED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID PetDBID{ LWOOBJID_EMPTY }; // optional
		int32_t nModerationStatus{};
	};

	// Server -> client.
	struct PetNameChanged : public NetGameMsg {
		PetNameChanged() : NetGameMsg(MessageType::Game::PET_NAME_CHANGED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t moderationStatus{};
		std::u16string name{};
		std::u16string ownerName{};
	};

	// Client -> server.
	struct CommandPet : public NetGameMsg {
		CommandPet() : NetGameMsg(MessageType::Game::COMMAND_PET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		NiPoint3 GenericPosInfo{};
		LWOOBJID ObjIDSource{};
		int32_t iPetCommandType{};
		int32_t iTypeID{};
		bool overrideObey{};
	};

	// Client -> server.
	struct DespawnPet : public NetGameMsg {
		DespawnPet() : NetGameMsg(MessageType::Game::DESPAWN_PET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bDeletePet{};
	};
};

#endif // PETMESSAGES_H
