#pragma once
#include "CppScripts.h"

class BaseConsoleTeleportServer
{
public:
	void BaseOnUse(Entity* self, Entity* user);
	void BaseOnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData);
	void UpdatePlayerTable(Entity* self, Entity* player, bool bAdd);
	bool CheckPlayerTable(Entity* self, Entity* player);
	void BaseOnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3);
	void TransferPlayer(Entity* self, Entity* player, int32_t altMapID);
	void BaseOnTimerDone(Entity* self, const std::string& timerName);

private:
	std::vector<LWOOBJID> m_Players = {};
};
