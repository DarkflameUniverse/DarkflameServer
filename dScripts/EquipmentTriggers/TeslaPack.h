#ifndef TESLAPACK_H
#define TESLAPACK_H

#include "CoilBackpackBase.h"

class TeslaPack : public CoilBackpackBase {
public:
	TeslaPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1001;
};

#endif  //!TESLAPACK_H
