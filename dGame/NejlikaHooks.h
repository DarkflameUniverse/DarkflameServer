#pragma once

namespace nejlika::NejlikaHooks
{

void InstallHooks();

void ItemDescription(Entity* entity, const SystemAddress& sysAddr, const std::string args);

}