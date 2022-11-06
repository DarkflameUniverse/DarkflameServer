#pragma once
#include "CppScripts.h"

class MaestromExtracticatorServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3);
	void CollectSample(Entity* self, LWOOBJID sampleObj);
	void PlayAnimAndReturnTime(Entity* self, std::string animID);
	void OnTimerDone(Entity* self, std::string timerName);

private:
	const std::string failAnim = "idle_maelstrom";
	const std::string collectAnim = "collect_maelstrom";
	const float defaultTime = 4.0f;
	const float destroyAfterNoSampleTime = 8.0f;
};
