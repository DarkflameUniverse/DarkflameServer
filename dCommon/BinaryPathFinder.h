#pragma once

#ifndef __BINARYPATHFINDER__H__
#define __BINARYPATHFINDER__H__

#include <filesystem>

class BinaryPathFinder {
private:
	static std::filesystem::path binaryDir;
public:
	static std::filesystem::path GetBinaryDir();
};

#endif  //!__BINARYPATHFINDER__H__
