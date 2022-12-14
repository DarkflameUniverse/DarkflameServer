#ifndef __TESLAPACK__H__
#define __TESLAPACK__H__

#include "CoilBackpackBase.h"

class TeslaPack : public CoilBackpackBase {
public:
	TeslaPack() : CoilBackpackBase(skillId, behaviorId) {};
private:
	static const uint32_t skillId = 1001;
	static const uint32_t behaviorId = 20917;
};

#endif  //!__TESLAPACK__H__
