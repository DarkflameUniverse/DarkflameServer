#pragma once
#include "CppScripts.h"
#include "RenderComponent.h"
/**
 * @brief Class for Brick Fury in Forbidden Valley.  Deals with mission progression
 * and killing the enemies.
 * 
 */
class FvHorsemenTrigger : public CppScripts::Script 
{
public:
	void OnStartup(Entity* self) override;
    void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
    void OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2,
                               int32_t param3) override;
};
