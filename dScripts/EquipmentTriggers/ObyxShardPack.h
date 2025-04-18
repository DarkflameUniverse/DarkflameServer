#ifndef __OBYXSHARDPACK__H__
#define __OBYXSHARDPACK__H__

#include "CoilBackpackBase.h"

class ObyxShardPack : public CoilBackpackBase {
public:
	ObyxShardPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1751;
};

#endif  //!__OBYXSHARDPACK__H__
