#pragma once
#include "CppScripts.h"

class ImgBrickConsoleQB : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void SpawnBrick(Entity* self);
	void SmashCanister(Entity* self);
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

public:
	static int32_t ResetBricks;
	static int32_t ResetConsole;
	static int32_t ResetInteract;
};
