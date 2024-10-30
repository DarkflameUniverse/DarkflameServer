#ifndef GEMPACK_H
#define GEMPACK_H

#include "CoilBackpackBase.h"

class GemPack : public CoilBackpackBase {
public:
	GemPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1488;
};

#endif  //!GEMPACK_H
