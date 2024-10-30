#ifndef EOBJECTBITS_H
#define EOBJECTBITS_H

#include <cstdint>

enum class eObjectBits : size_t {
	PERSISTENT = 32,
	CLIENT = 46,
	SPAWNED = 58,
	CHARACTER = 60
};

#endif  //!EOBJECTBITS_H
