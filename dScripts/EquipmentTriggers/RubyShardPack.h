#ifndef __RUBYSHARDPACK__H__
#define __RUBYSHARDPACK__H__

#include "CoilBackpackBase.h"

class RubyShardPack : public CoilBackpackBase {
public:
	RubyShardPack() : CoilBackpackBase(skillId) {};
private:
	static const uint32_t skillId = 1750;
};

#endif  //!__RUBYSHARDPACK__H__
