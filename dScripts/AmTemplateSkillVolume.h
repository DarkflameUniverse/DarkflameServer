#pragma once
#include "CppScripts.h"

class AmTemplateSkillVolume : public CppScripts::Script
{
public:
    /**
     * @brief Script for checking if player cast a skill near Neido in Crux Prime???
     * 
     * @param self This Entity.
     * @param caster The Caster Entity.
     * @param message The Casted Ability.
     */
    void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
};
