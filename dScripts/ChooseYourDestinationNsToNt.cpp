#include "ChooseYourDestinationNsToNt.h"
#include "Character.h"
#include "GameMessages.h"

bool ChooseYourDestinationNsToNt::CheckChoice(Entity* self, Entity* player) {
	const auto choiceZoneID = self->GetVar<int32_t>(u"choiceZone");
	const auto newZoneID = self->GetVar<int32_t>(u"currentZone");

	if (newZoneID == choiceZoneID) {
		auto* character = player->GetCharacter();

		if (character == nullptr) {
			return false;
		}

		if (character->HasBeenToWorld(1900)) {
			return true;
		}

		self->SetVar(u"transferZoneID", GeneralUtils::to_u16string(1200));
		self->SetVar<std::u16string>(u"teleportString", u"UI_TRAVEL_TO_NS");

		return false;
	}

	return false;
}

void ChooseYourDestinationNsToNt::SetDestination(Entity* self, Entity* player) {
	const auto currentMap = self->GetVar<int32_t>(u"currentZone");
	auto newMap = self->GetVar<int32_t>(u"choiceZone");

	if (currentMap == newMap) {
		newMap = 1200;
	}

	self->SetVar(u"transferZoneID", GeneralUtils::to_u16string(newMap));
}

void ChooseYourDestinationNsToNt::BaseChoiceBoxRespond(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {
	if (button != -1) {
		const auto newMapStr = GeneralUtils::UTF16ToWTF8(buttonIdentifier).substr(7, -1);

		int32_t newMap = 0;
		if (!GeneralUtils::TryParse(newMapStr, newMap)) {
			return;
		}

		std::u16string strText = u"";

		if (newMap == 1200) {
			strText = u"UI_TRAVEL_TO_NS";
		} else {
			strText = u"UI_TRAVEL_TO_NEXUS_TOWER";
		}

		self->SetVar(u"teleportString", strText);
		self->SetVar(u"transferZoneID", GeneralUtils::to_u16string(newMap));

		GameMessages::SendDisplayMessageBox(sender->GetObjectID(), true, self->GetObjectID(), u"TransferBox", 0, strText, u"", sender->GetSystemAddress());
	} else {
		GameMessages::SendTerminateInteraction(sender->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
	}
}
