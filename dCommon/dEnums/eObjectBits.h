#ifndef __EOBJECTBITS__H__
#define __EOBJECTBITS__H__

#include <cstdint>

enum class eObjectBits : size_t {
	PERSISTENT = 32,
	CLIENT = 46,
	SPAWNED = 58,
	CHARACTER = 60
};

#endif  //!__EOBJECTBITS__H__
