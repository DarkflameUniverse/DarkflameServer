#pragma once
#include "CppScripts.h"
#include "ChooseYourDestinationNsToNt.h"
#include "BaseConsoleTeleportServer.h"

class NtConsoleTeleportServer : public CppScripts::Script, BaseConsoleTeleportServer
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;
	void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;

private:
	int32_t m_ChoiceZoneID = 1800;
	std::string m_SpawnPoint = "NS_LW";
	std::u16string m_TeleportAnim = u"lup-teleport";
	std::u16string m_TeleportString = u"UI_TRAVEL_TO_CRUX_PRIME";
};
