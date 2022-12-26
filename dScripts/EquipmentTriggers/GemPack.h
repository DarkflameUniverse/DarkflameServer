#ifndef __GEMPACK__H__
#define __GEMPACK__H__

#include "CoilBackpackBase.h"

class GemPack : public CoilBackpackBase {
public:
	GemPack() : CoilBackpackBase(skillId, behaviorId) {};
private:
	static const uint32_t skillId = 1488;
	static const uint32_t behaviorId = 36779;
};

#endif  //!__GEMPACK__H__
