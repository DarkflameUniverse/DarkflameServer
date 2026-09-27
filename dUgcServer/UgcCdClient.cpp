#include "UgcCdClient.h"

#include <optional>

#include "CDClientDatabase.h"
#include "UgcModular.h"

namespace {
	constexpr int32_t MODULE_COMPONENT = 28;
	constexpr int32_t RENDER_COMPONENT = 2;
}

namespace UgcCdClient {
	bool GatherModular(const std::string& modules, UgcJobs::ModularInput& input, std::string& error) {
		const auto lots = UgcModular::ParseModuleLots(modules);
		if (lots.empty()) {
			error = "no modules in \"" + modules + "\"";
			return false;
		}
		std::optional<int32_t> buildType;
		for (const auto lot : lots) {
			auto moduleQuery = CDClientDatabase::CreatePreppedStmt(
				"SELECT m.partCode, m.buildType, m.xml FROM ComponentsRegistry cr JOIN ModuleComponent m ON m.id = cr.component_id "
				"WHERE cr.id = ? AND cr.component_type = ? LIMIT 1;");
			moduleQuery.bind(1, static_cast<int>(lot));
			moduleQuery.bind(2, MODULE_COMPONENT);
			auto moduleRow = moduleQuery.execQuery();
			if (moduleRow.eof()) {
				error = "LOT " + std::to_string(lot) + " is not a module";
				return false;
			}
			UgcJobs::ModuleInput module;
			module.lot = lot;
			module.partCode = static_cast<uint32_t>(moduleRow.getIntField("partCode", 0));
			module.moduleXml = moduleRow.getStringField("xml", "");
			if (!buildType) buildType = moduleRow.getIntField("buildType", 0);
			input.buildType = *buildType;
	
			auto renderQuery = CDClientDatabase::CreatePreppedStmt(
				"SELECT rc.render_asset FROM ComponentsRegistry cr JOIN RenderComponent rc ON rc.id = cr.component_id "
				"WHERE cr.id = ? AND cr.component_type = ? LIMIT 1;");
			renderQuery.bind(1, static_cast<int>(lot));
			renderQuery.bind(2, RENDER_COMPONENT);
			auto renderRow = renderQuery.execQuery();
			if (!renderRow.eof()) module.renderAsset = renderRow.getStringField("render_asset", "");
			input.modules.push_back(std::move(module));
		}
	
		auto buildQuery = CDClientDatabase::CreatePreppedStmt("SELECT xml FROM ModularBuildComponent WHERE buildType = ? ORDER BY id LIMIT 1;");
		buildQuery.bind(1, *buildType);
		auto buildRow = buildQuery.execQuery();
		if (buildRow.eof()) {
			error = "no ModularBuildComponent for build type " + std::to_string(*buildType);
			return false;
		}
		input.buildXml = buildRow.getStringField("xml", "");
		return true;
	}
}
