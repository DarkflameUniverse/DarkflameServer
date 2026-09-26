#include "ActivityMessages.h"

#include "BitStreamUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"

namespace GameMessages {
	void ActivityStop::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bExit);
		bitStream.Write(bUserCancel);
	}

	bool ActivityStop::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bExit));
		VALIDATE_READ(bitStream.Read(bUserCancel));
		return true;
	}

	void ActivityPause::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bPause);
	}

	bool ActivityPause::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bPause));
		return true;
	}

	void StartActivityTime::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(startTime);
	}

	bool StartActivityTime::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(startTime));
		return true;
	}

	void RequestActivityEnter::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bStart);
		bitStream.Write(userID);
	}

	bool RequestActivityEnter::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bStart));
		VALIDATE_READ(bitStream.Read(userID));
		return true;
	}

	void RequestActivityExit::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bUserCancel);
		bitStream.Write(userID);
	}

	bool RequestActivityExit::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bUserCancel));
		VALIDATE_READ(bitStream.Read(userID));
		return true;
	}

	void RequestActivityExit::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!bUserCancel) return;

		auto* player = Game::entityManager->GetEntity(userID);
		if (!player) return;
		entity.RequestActivityExit(&entity, userID, bUserCancel);
	}

	void ShowActivityCountdown::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bPlayAdditionalSound);
		bitStream.Write(bPlayCountdownSound);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sndName);
		bitStream.Write(stateToPlaySoundOn);
	}

	bool ShowActivityCountdown::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bPlayAdditionalSound));
		VALIDATE_READ(bitStream.Read(bPlayCountdownSound));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sndName));
		VALIDATE_READ(bitStream.Read(stateToPlaySoundOn));
		return true;
	}
}
