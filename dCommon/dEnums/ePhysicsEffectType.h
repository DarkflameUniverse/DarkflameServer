#ifndef __EPHYSICSEFFECTTYPE__H__
#define __EPHYSICSEFFECTTYPE__H__


#include <cstdint>

enum class ePhysicsEffectType : uint32_t {
	PUSH,
	ATTRACT,
	REPULSE,
	GRAVITY_SCALE,
	FRICTION
};

#endif  //!__EPHYSICSEFFECTTYPE__H__
