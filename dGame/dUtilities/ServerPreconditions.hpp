#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "dCommonVars.h"
#include "Preconditions.h"

class Entity;

namespace ServerPreconditions
{

extern std::unordered_map<LOT, std::vector<std::pair<bool, PreconditionExpression>>> m_Preconditions;

extern std::unordered_map<LWOOBJID, LWOOBJID> m_SoloActors;

extern std::unordered_map<LWOOBJID, std::unordered_set<LWOOBJID>> m_ExcludeForPlayer;

void LoadPreconditions(std::string file);

bool CheckPreconditions(Entity* target, Entity* entity);

bool IsSoloActor(LWOOBJID actor);

bool IsActingFor(LWOOBJID actor, LWOOBJID target);

void AddSoloActor(LWOOBJID actor, LWOOBJID target);

void RemoveSoloActor(LWOOBJID actor);

void AddExcludeFor(LWOOBJID player, LWOOBJID target);

void RemoveExcludeFor(LWOOBJID player, LWOOBJID target);

bool IsExcludedFor(LWOOBJID player, LWOOBJID target);

}