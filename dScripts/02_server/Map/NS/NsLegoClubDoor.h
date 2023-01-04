#pragma once
#include "CppScripts.h"
#include "ChooseYourDestinationNsToNt.h"
#include "BaseConsoleTeleportServer.h"

class NsLegoClubDoor : public CppScripts::Script, ChooseYourDestinationNsToNt, BaseConsoleTeleportServer
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;
	void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;

private:
	int32_t m_ChoiceZoneID = 1700;
	std::string m_SpawnPoint = "NS_LEGO_Club";
	std::u16string m_TeleportAnim = u"lup-teleport";
	std::u16string m_TeleportString = u"ROCKET_TOOLTIP_USE_THE_GATEWAY_TO_TRAVEL_TO_LUP_WORLD";
	AMFArrayValue args = {};
	AMFArrayValue* options = {};
};
