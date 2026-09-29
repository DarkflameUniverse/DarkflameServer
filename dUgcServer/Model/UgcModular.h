#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "NifFile.h"
#include "UgcModel.h"

/**
 * Modular builds (cars, rockets) put together from their modules the way the CDClient describes it:
 * ModularBuildComponent.xml has the topology (a root part and which part connects to which named location of which
 * other part), ModuleComponent.xml each module's connection offsets, and each module's render asset has the named
 * attach-point nodes. Pure.
 */
namespace UgcModular {
	struct Connection {
		uint32_t part{};           // the part the location is on
		std::string location;      // node name on that part
		uint32_t connectingPart{}; // the part placed there
	};

	struct BuildInfo {
		uint32_t rootPart{};
		uint32_t parts{};
		std::vector<Connection> connections;
		glm::mat4 additionalRotation{ 1.0f }; // Placement/AdditionalModelRotation
	};

	// ModularBuildComponent.xml; nullopt when it has no topology
	std::optional<BuildInfo> ParseBuild(std::string_view xml);

	// ModuleComponent.xml's connection offsets, by location name
	std::map<std::string, glm::vec3> ParseModuleConnections(std::string_view xml);

	// The LOTs in a ugc_modular_build.ldf_config ("1:4713+1:4714+1:4715", also with ';' or ',' between)
	std::vector<uint32_t> ParseModuleLots(std::string_view ldf);

	struct Module {
		uint32_t partCode{};
		NifFile::Model nif;
		std::map<std::string, glm::vec3> connections; // from ParseModuleConnections
	};

	/**
	 * The modules placed per the build's topology, starting at the root part: a connection puts the connecting part
	 * where the parent's node of the location's name is (or the parent module's connection offset of that name when
	 * its mesh has no such node), less where the connecting part's own node of that name is, if it has one.
	 * Parts without a module are skipped; `warnings` says what was.
	 */
	UgcModel::Model Assemble(const BuildInfo& build, const std::vector<Module>& modules, std::string& warnings);
}
