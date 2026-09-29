#ifndef MODULARBUILDIDMIGRATION_H
#define MODULARBUILDIDMIGRATION_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "dCommonVars.h"

namespace tinyxml2 {
	class XMLDocument;
}

/**
 * Cars and rockets built before builds were recorded: saved items with modules (x@ma, assemblyPartLOTs) and no subkey,
 * so they have no ugc_modular_build row and the client has no blueprint id to ask for their icon with. The migration
 * (dlu/mysql/98_modular_build_ids.sql, dlu/sqlite/81_modular_build_ids.sql) gives each what a new build gets
 * (ModularBuildFinish) and what InventoryComponent gives one when its character loads: a persistent id as its subkey
 * and a ugc_modular_build row with its modules and owner. Only modular builds have assemblyPartLOTs.
 */
namespace ModularBuildIdMigration {
	struct NewBuild {
		LWOOBJID id{};
		std::string modules; // "1:8129+1:8130+...", as ugc_modular_build.ldf_config
	};

	// Gives every item of a character's saved XML with modules and no subkey the next id as its subkey; the builds to insert
	std::vector<NewBuild> AssignIds(tinyxml2::XMLDocument& document, const std::function<LWOOBJID()>& nextId);

	void Run();
};

#endif //!MODULARBUILDIDMIGRATION_H
