#pragma once
#include "CppScripts.h"

class ZoneSGServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnActivityStateChangeRequest(Entity* self, LWOOBJID senderID, int32_t value1,
		int32_t value2, const std::u16string& stringValue) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
private:
	std::u16string CannonIDVariable = u"CannonID";
};
