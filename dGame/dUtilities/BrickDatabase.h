#pragma once

#include "Entity.h"

class Brick;
using BrickList = std::vector<Brick>;
using LxfmlPath = std::string;

namespace BrickDatabase {
	const BrickList& GetBricks(const LxfmlPath& lxfmlPath);

	std::unordered_map<LxfmlPath, BrickList> m_Cache;
	const BrickList emptyCache;
};
