#pragma once
//#include "NjPetSpawnerServer.h"
#include "SpawnPetBaseServer.h"

class NjEarthDragonPetServer : public SpawnPetBaseServer {
    void SetVariables(Entity* self) override;
};
