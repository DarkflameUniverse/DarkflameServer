#pragma once
#include "CppScripts.h"

class NtVentureCannonServer : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
	void EnterCannonEnded(Entity* self, Entity* player);
	void ExitCannonEnded(Entity* self, Entity* player);
	void UnlockCannonPlayer(Entity* self, Entity* player);
	void FirePlayer(Entity* self, Entity* player);
};
