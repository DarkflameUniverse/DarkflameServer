#ifndef __EBASICATTACKSUCCESSTYPES__H__
#define __EBASICATTACKSUCCESSTYPES__H__

#include <cstdint>

enum class eBasicAttackSuccessTypes : uint8_t {
	SUCCESS = 1,
	FAILARMOR,
	FAILIMMUNE
};

#endif  //!__EBASICATTACKSUCCESSTYPES__H__
