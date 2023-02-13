#ifndef __GEMPACK__H__
#define __GEMPACK__H__

#include "CoilBackpackBase.h"

class GemPack : public CoilBackpackBase {
public:
	GemPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1488;
};

#endif  //!__GEMPACK__H__
