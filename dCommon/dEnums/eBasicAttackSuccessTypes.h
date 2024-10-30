#ifndef EBASICATTACKSUCCESSTYPES_H
#define EBASICATTACKSUCCESSTYPES_H

#include <cstdint>

enum class eBasicAttackSuccessTypes : uint8_t {
	SUCCESS = 1,
	FAILARMOR,
	FAILIMMUNE
};

#endif  //!EBASICATTACKSUCCESSTYPES_H
