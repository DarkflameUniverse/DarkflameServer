#include "CDModularBuildComponentTable.h"

#include <regex>
#include <string>

namespace {
	std::optional<int32_t> ValueOf(const std::string& xml, const std::string& element) {
		const std::regex pattern("<" + element + R"re(\s+value\s*=\s*"(-?\d+)")re");
		std::smatch match;
		if (!std::regex_search(xml, match, pattern)) return std::nullopt;
		return std::stoi(match[1].str());
	}
}

void CDModularBuildComponentTable::ParseXml(const std::string_view xmlView, CDModularBuildComponent& build) {
	const std::string xml(xmlView);
	build.numberOfParts = static_cast<uint32_t>(ValueOf(xml, "numberOfParts").value_or(0));

	const auto rootPart = ValueOf(xml, "rootPart");
	if (!rootPart) return;

	// Each <Module> has a PartCode and an ExamplePartLOT
	const std::regex module(R"(<Module\b[\s\S]*?</Module>)");
	for (auto it = std::sregex_iterator(xml.begin(), xml.end(), module); it != std::sregex_iterator(); ++it) {
		const auto text = it->str();
		if (ValueOf(text, "PartCode") == rootPart) {
			build.rootPartExampleLOT = ValueOf(text, "ExamplePartLOT").value_or(LOT_NULL);
			return;
		}
	}
}

void CDModularBuildComponentTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ModularBuildComponent;");
	while (!tableData.eof()) {
		CDModularBuildComponent entry;
		entry.id = tableData.getIntField("id", 0);
		entry.buildType = tableData.getIntField("buildType", 0);
		entry.createdLOT = tableData.getIntField("createdLOT", LOT_NULL);
		ParseXml(tableData.getStringField("xml", ""), entry);
		entries.push_back(entry);
		tableData.nextRow();
	}
	tableData.finalize();
}

std::optional<CDModularBuildComponent> CDModularBuildComponentTable::FindByNumberOfParts(const std::vector<CDModularBuildComponent>& builds, const uint32_t numberOfParts) {
	for (const auto& build : builds) {
		if (build.numberOfParts == numberOfParts && build.createdLOT != LOT_NULL) return build;
	}
	return std::nullopt;
}

std::optional<CDModularBuildComponent> CDModularBuildComponentTable::GetByNumberOfParts(const uint32_t numberOfParts) const {
	return FindByNumberOfParts(GetEntries(), numberOfParts);
}
