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

	[[nodiscard]] Result NormalizePosition(const std::string_view data);
};

#endif //!LXFML_H
