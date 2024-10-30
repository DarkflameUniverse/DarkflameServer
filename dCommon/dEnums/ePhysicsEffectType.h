#ifndef EPHYSICSEFFECTTYPE_H
#define EPHYSICSEFFECTTYPE_H


#include <cstdint>

enum class ePhysicsEffectType : uint32_t {
	PUSH,
	ATTRACT,
	REPULSE,
	GRAVITY_SCALE,
	FRICTION
};

#endif  //!EPHYSICSEFFECTTYPE_H
