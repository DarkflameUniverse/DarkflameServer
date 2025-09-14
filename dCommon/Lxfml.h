// Darkflame Universe
// Copyright 2025

#ifndef LXFML_H
#define LXFML_H

#include <string>
#include <string_view>
#include <vector>

#include "NiPoint3.h"

namespace Lxfml {
	struct Result {
		std::string lxfml;
		NiPoint3 center;
	};

	// Normalizes a LXFML model to be positioned relative to its local 0, 0, 0 rather than a game worlds 0, 0, 0.
	// Returns a struct of its new center and the updated LXFML containing these edits.
	[[nodiscard]] Result NormalizePosition(const std::string_view data, const NiPoint3& curPosition = NiPoint3Constant::ZERO);
	[[nodiscard]] std::vector<Result> Split(const std::string_view data, const NiPoint3& curPosition = NiPoint3Constant::ZERO);

	// The base LXFML xml file to use when creating new models.
	static inline std::string base = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0">
<Meta>
    <Application name="LEGO Universe" versionMajor="0" versionMinor="0"/>
    <Brand name="LEGOUniverse"/>
    <BrickSet version="457"/>
</Meta>
<Bricks>
</Bricks>
<RigidSystems>
</RigidSystems>
<GroupSystems>
    <GroupSystem>
    </GroupSystem>
</GroupSystems>
</LXFML>)";

	// these are only for the migrations due to a bug in one of the implementations.
	[[nodiscard]] Result NormalizePositionOnlyFirstPart(const std::string_view data);
	[[nodiscard]] Result NormalizePositionAfterFirstPart(const std::string_view data, const NiPoint3& position);
};

#endif //!LXFML_H
