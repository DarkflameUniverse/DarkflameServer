#ifndef __ICHARACTERREPUTATION__H__
#define __ICHARACTERREPUTATION__H__

#include <cstdint>

#include "dCommonVars.h"

class ICharacterReputation {
public:
	virtual int64_t GetCharacterReputation(const LWOOBJID charId) = 0;
	virtual void SetCharacterReputation(const LWOOBJID charId, const int64_t reputation) = 0;
};

#endif  //!__ICHARACTERREPUTATION__H__
