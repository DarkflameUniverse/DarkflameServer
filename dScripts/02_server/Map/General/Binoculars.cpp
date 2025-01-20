#include "Binoculars.h"

#include "GameMessages.h"
#include "Game.h"
#include "dZoneManager.h"

void Binoculars::OnUse(Entity* self, Entity* user) {
	const auto number = self->GetVarAsString(u"number");

	int32_t flag = std::stoi(std::to_string(Game::zoneManager->GetZoneID().GetMapID()).substr(0, 2) + number);

	GameMessages::GetFlag getFlag{};
	getFlag.target = user->GetObjectID();
	getFlag.iFlagId = flag;
	SEND_ENTITY_MSG(getFlag);

	if (!getFlag.bFlag) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = user->GetObjectID();
		setFlag.iFlagId = flag;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);

		GameMessages::SendFireEventClientSide(self->GetObjectID(), user->GetSystemAddress(), u"achieve", LWOOBJID_EMPTY, 0, -1, LWOOBJID_EMPTY);
	}
}
