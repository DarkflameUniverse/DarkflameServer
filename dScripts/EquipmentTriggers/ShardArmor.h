#ifndef __SHARDARMOR__H__
#define __SHARDARMOR__H__

#include "CoilBackpackBase.h"

class ShardArmor : public CoilBackpackBase {
public:
	ShardArmor() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1249;
};

#endif  //!__SHARDARMOR__H__
