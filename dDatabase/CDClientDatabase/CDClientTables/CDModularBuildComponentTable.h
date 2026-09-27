#pragma once
#include "CDTable.h"
#include "dCommonVars.h"

#include <optional>
#include <string_view>

// A modular build (rocket, car): ModularBuildComponent with what DLU needs from its xml
struct CDModularBuildComponent {
	uint32_t id{};
	int32_t buildType{};
	LOT createdLOT{ LOT_NULL };       // the item the finished build becomes
	uint32_t numberOfParts{};          // <topology><numberOfParts value>
	LOT rootPartExampleLOT{ LOT_NULL }; // <Module><ExamplePartLOT> of the module whose PartCode is <rootPart value>
};

class CDModularBuildComponentTable : public CDTable<CDModularBuildComponentTable, std::vector<CDModularBuildComponent>> {
public:
	void LoadValuesFromDatabase();

	// The first build made of this many parts
	std::optional<CDModularBuildComponent> GetByNumberOfParts(uint32_t numberOfParts) const;

	// Whether a LOT is the item some build becomes (a car or rocket)
	bool IsCreatedLot(LOT lot) const;

	// Fills numberOfParts and rootPartExampleLOT from a ModularBuildComponent.xml
	static void ParseXml(std::string_view xml, CDModularBuildComponent& build);
	static std::optional<CDModularBuildComponent> FindByNumberOfParts(const std::vector<CDModularBuildComponent>& builds, uint32_t numberOfParts);
};
