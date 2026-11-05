// Darkflame Universe
// Copyright 2025

#ifndef BRICKBOUNDS_H
#define BRICKBOUNDS_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "NiPoint3.h"

class AssetManager;

// The bounding boxes of the client's bricks, from its brickdb (res/brickdb.zip)
namespace BrickBounds {
	// A brick's box around its own origin
	struct Bounds {
		NiPoint3 min;
		NiPoint3 max;
	};

	// Design ID -> its box
	using DesignBounds = std::unordered_map<uint32_t, Bounds>;

	// The <Bounding><AABB/> of one primitive (Primitives/<designID>.xml)
	[[nodiscard]] std::optional<Bounds> ParsePrimitive(std::string_view xml);

	// The box of every primitive in a brickdb.zip. A design listed only as another primitive's alias gets that
	// primitive's box.
	[[nodiscard]] DesignBounds ParseBrickDb(std::string_view zip);

	// Reads brickdb.zip through `assets`; empty if it can't be read
	[[nodiscard]] DesignBounds Load(const AssetManager& assets);

	// The boxes read by Load the first time this is called, kept for every later call (`assets` is only used then).
	// Empty without `assets`.
	[[nodiscard]] const DesignBounds& Get(const AssetManager* assets);
};

#endif //!BRICKBOUNDS_H
