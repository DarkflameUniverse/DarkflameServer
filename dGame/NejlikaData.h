#pragma once

#include <cstdint>
#include <vector>

#include "ModifierNameTemplate.h"
#include "EntityTemplate.h"
#include "AdditionalItemData.h"
#include "AdditionalEntityData.h"
#include "UpgradeTemplate.h"
#include "Lookup.h"

namespace nejlika::NejlikaData
{

const std::unordered_map<ModifierNameType, std::vector<ModifierNameTemplate>>& GetModifierNameTemplates();

const std::vector<ModifierNameTemplate>& GetModifierNameTemplates(ModifierNameType type);

const std::optional<AdditionalItemData*> GetAdditionalItemData(LWOOBJID id);

const std::optional<AdditionalEntityData*> GetAdditionalEntityData(LWOOBJID id);

const std::optional<EntityTemplate*> GetEntityTemplate(LOT lot);

const std::optional<UpgradeTemplate*> GetUpgradeTemplate(LOT lot);

void SetAdditionalItemData(LWOOBJID id, AdditionalItemData data);

void SetAdditionalEntityData(LWOOBJID id, AdditionalEntityData data);

void UnsetAdditionalItemData(LWOOBJID id);

void UnsetAdditionalEntityData(LWOOBJID id);

void LoadNejlikaData();

const Lookup& GetLookup();

}
