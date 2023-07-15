#ifndef __BRICKDATABASE__H__
#define __BRICKDATABASE__H__

#pragma once

#include "Entity.h"

class Brick;
using BrickList = std::vector<Brick>;
using LxfmlPath = std::string;

namespace BrickDatabase {
	const BrickList& GetBricks(const LxfmlPath& lxfmlPath);

	extern std::unordered_map<LxfmlPath, BrickList> m_Cache;
	extern const BrickList emptyCache;
};

#endif  //!__BRICKDATABASE__H__
