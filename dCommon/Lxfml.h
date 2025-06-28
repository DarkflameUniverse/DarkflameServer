// Darkflame Universe
// Copyright 2025

#ifndef LXFML_H
#define LXFML_H

#include <string>
#include <string_view>

#include "NiPoint3.h"

namespace Lxfml {
	struct Result {
		std::string lxfml;
		NiPoint3 center;
	};

	// Normalizes a LXFML model to be positioned relative to its local 0, 0, 0 rather than a game worlds 0, 0, 0.
	// Returns a struct of its new center and the updated LXFML containing these edits.
	[[nodiscard]] Result NormalizePosition(const std::string_view data, const NiPoint3& curPosition = NiPoint3Constant::ZERO);

	// these are only for the migrations due to a bug in one of the implementations.
	[[nodiscard]] Result NormalizePositionOnlyFirstPart(const std::string_view data);
	[[nodiscard]] Result NormalizePositionAfterFirstPart(const std::string_view data, const NiPoint3& position);
};

#endif //!LXFML_H
