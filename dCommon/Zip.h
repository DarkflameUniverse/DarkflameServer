// Darkflame Universe
// Copyright 2025

#ifndef ZIP_H
#define ZIP_H

#include <functional>
#include <string_view>

namespace Zip {
	// Calls `visit` with the name and contents of every entry of the zip archive `zip` whose name `wanted` accepts, in
	// the archive's order. Stored and deflated entries are read; others, and entries that fail to read, are skipped.
	// Returns false when the archive's directory can't be read.
	bool ForEachEntry(std::string_view zip, const std::function<bool(std::string_view name)>& wanted,
		const std::function<void(std::string_view name, std::string_view data)>& visit);
};

#endif //!ZIP_H
