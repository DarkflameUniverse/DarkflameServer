#ifndef __ONYXSHARDPACK__H__
#define __ONYXSHARDPACK__H__

#include "CoilBackpackBase.h"

class OnyxShardPack : public CoilBackpackBase {
public:
	OnyxShardPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1751;
};

#endif  //!__SHARDARMOR__H__
