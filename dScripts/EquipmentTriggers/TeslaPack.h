#ifndef __TESLAPACK__H__
#define __TESLAPACK__H__

#include "CoilBackpackBase.h"

class TeslaPack : public CoilBackpackBase {
public:
	TeslaPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1001;
};

#endif  //!__TESLAPACK__H__
