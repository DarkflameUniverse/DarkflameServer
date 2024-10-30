#ifndef SHARDARMOR_H
#define SHARDARMOR_H

#include "CoilBackpackBase.h"

class ShardArmor : public CoilBackpackBase {
public:
	ShardArmor() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1249;
};

#endif  //!SHARDARMOR_H
