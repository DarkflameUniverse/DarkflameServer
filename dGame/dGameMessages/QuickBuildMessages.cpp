#include "QuickBuildMessages.h"

#include "BitStreamUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "QuickBuildComponent.h"

namespace GameMessages {
	void RebuildNotifyState::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(prevState);
		bitStream.Write(state);
		bitStream.Write(player);
	}

	bool RebuildNotifyState::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(prevState));
		VALIDATE_READ(bitStream.Read(state));
		VALIDATE_READ(bitStream.Read(player));
		return true;
	}

	void EnableRebuild::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bEnable);
		bitStream.Write(bFail);
		bitStream.Write(bSuccess);
		BitStreamUtils::WriteOptional(bitStream, eFailReason, eQuickBuildFailReason::NOT_GIVEN);
		bitStream.Write(fDuration);
		bitStream.Write(user);
	}

	bool EnableRebuild::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bEnable));
		VALIDATE_READ(bitStream.Read(bFail));
		VALIDATE_READ(bitStream.Read(bSuccess));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, eFailReason, eQuickBuildFailReason::NOT_GIVEN));
		VALIDATE_READ(bitStream.Read(fDuration));
		VALIDATE_READ(bitStream.Read(user));
		return true;
	}

	void RebuildCancel::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bEarlyRelease);
		bitStream.Write(userID);
	}

	bool RebuildCancel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bEarlyRelease));
		VALIDATE_READ(bitStream.Read(userID));
		return true;
	}

	void RebuildCancel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* quickBuildComponent = entity.GetComponent<QuickBuildComponent>();
		if (!quickBuildComponent) return;

		quickBuildComponent->CancelQuickBuild(Game::entityManager->GetEntity(userID), eQuickBuildFailReason::CANCELED_EARLY);
	}
}
