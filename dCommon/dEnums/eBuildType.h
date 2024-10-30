#ifndef EBUILDTYPE_H
#define EBUILDTYPE_H

#include <cstdint>

enum class eBuildType :uint32_t {
	NOWHERE,
	IN_WORLD,
	ON_PROPERTY
};

#endif  //!EBUILDTYPE_H
