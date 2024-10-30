#pragma once

#ifndef BINARYPATHFINDER_H
#define BINARYPATHFINDER_H

#include <filesystem>

class BinaryPathFinder {
private:
	static std::filesystem::path binaryDir;
public:
	static std::filesystem::path GetBinaryDir();
};

#endif  //!BINARYPATHFINDER_H
