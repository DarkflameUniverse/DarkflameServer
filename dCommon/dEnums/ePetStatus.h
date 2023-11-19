#ifndef __EPETSTATUS__H__
#define __EPETSTATUS__H__

#include <cstdint>

enum ePetStatus : uint32_t {
	NONE,
	UNKNOWN1 = 0x1,
	UNKNOWN2 = 0x2,
	UNKNOWN3 = 0x4,
	UNKNOWN4 = 0x8,
	BEING_TAMED = 0x10,
	IS_NOT_WAITING = 0x20, // Right name? - used to be decimal 20
	PLAY_SPAWN_ANIM = 0x80, 
	TAMEABLE = 0x4000000
};

#endif  //!__EPETSTATUS__H__
