#pragma once

#include "../GameMessage.h"
#include "../GameMessages.h"

class RequestResurrect : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_RESURRECT; }

	void Handle() {
		GameMessages::SendResurrect(associate);
	}
};