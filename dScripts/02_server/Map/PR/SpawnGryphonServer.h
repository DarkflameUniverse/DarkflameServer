#pragma once
#include "SpawnPetBaseServer.h"

class SpawnGryphonServer : public SpawnPetBaseServer {
	void SetVariables(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
};
