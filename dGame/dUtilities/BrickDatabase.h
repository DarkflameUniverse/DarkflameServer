#ifndef BRICKDATABASE_H
#define BRICKDATABASE_H

#pragma once

#include "Entity.h"

class Brick;
using BrickList = std::vector<Brick>;
using LxfmlPath = std::string;

namespace BrickDatabase {
	const BrickList& GetBricks(const LxfmlPath& lxfmlPath);
};

#endif  //!BRICKDATABASE_H
