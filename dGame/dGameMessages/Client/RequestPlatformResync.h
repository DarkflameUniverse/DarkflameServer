#pragma once

#include "../GameMessage.h"
#include "../GameMessages.h"

class RequestPlatformResync : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_PLATFORM_RESYNC; }

	void Handle() {
		if (associate->GetLOT() == 6267 || associate->GetLOT() == 16141) return;
		GameMessages::SendPlatformResync(associate, sysAddr);
	}
};