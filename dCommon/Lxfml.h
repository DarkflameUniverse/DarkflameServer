// Darkflame Universe
// Copyright 2025

#ifndef LXFML_H
#define LXFML_H

#include <string>
#include <string_view>
#include <vector>

#include "BrickBounds.h"
#include "NiPoint3.h"

namespace Lxfml {
	struct Result {
		std::string lxfml;
		NiPoint3 center;
	};

	// Normalizes a LXFML model to be positioned relative to its local 0, 0, 0 rather than a game worlds 0, 0, 0.
	// Returns a struct of its new center and the updated LXFML containing these edits.
	// The new center is the bottom middle of the box around the model's bricks, each brick's box taken from `bounds` by
	// its design ID (a brick with no box there counts as its bones' origins).
	[[nodiscard]] Result NormalizePosition(const std::string_view data, const NiPoint3& curPosition = NiPoint3Constant::ZERO, const BrickBounds::DesignBounds& bounds = {});
	[[nodiscard]] std::vector<Result> Split(const std::string_view data, const NiPoint3& curPosition = NiPoint3Constant::ZERO, const BrickBounds::DesignBounds& bounds = {});

	// What a model is made of: every brick's design id (one per Brick, in file order) and the box around the bricks'
	// origins (their bones' positions; zero with no bricks). Brick shapes aren't known here, so the box is the
	// smallest one holding every brick's origin.
	struct Contents {
		std::vector<uint32_t> designIds;
		NiPoint3 boxMin{};
		NiPoint3 boxMax{};
	};
	[[nodiscard]] Contents ReadContents(const std::string_view data);

	// these are only for the migrations due to a bug in one of the implementations.
	[[nodiscard]] Result NormalizePositionOnlyFirstPart(const std::string_view data);
	[[nodiscard]] Result NormalizePositionAfterFirstPart(const std::string_view data, const NiPoint3& position);
};

#endif //!LXFML_H
