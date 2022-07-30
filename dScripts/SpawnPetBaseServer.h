#pragma once
#include "CppScripts.h"

/**
 * Base class that can be used to spawn pets based on interactions, requires the following variables from a child script:
 * - petLOT, LOT: lot of the pet to spawn
 * - petType, string: name of the type of pet to spawn
 * - maxPets, uint32_t: max number of pets to spawn at a time
 * - spawnAnim, wstring: animation to play when spawning a pet
 * - spawnCinematic, u16string: optional string of a cinematic to play
 */
class SpawnPetBaseServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	virtual void SetVariables(Entity* self) {};
private:
	static bool CheckNumberOfPets(Entity* self, Entity* user);
};
