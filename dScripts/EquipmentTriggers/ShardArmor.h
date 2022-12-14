#ifndef __SHARDARMOR__H__
#define __SHARDARMOR__H__

#include "CoilBackpackBase.h"

class ShardArmor : public CoilBackpackBase {
public:
	ShardArmor() : CoilBackpackBase(skillId, behaviorId) {};
private:
	static const uint32_t skillId = 1249;
	static const uint32_t behaviorId = 29086;
};

#endif  //!__SHARDARMOR__H__
