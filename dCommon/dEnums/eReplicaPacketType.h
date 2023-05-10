#ifndef __EREPLICAPACKETTYPE__H__
#define __EREPLICAPACKETTYPE__H__

#include <cstdint>

enum class eReplicaPacketType : uint8_t {
	CONSTRUCTION,
	SERIALIZATION,
	DESTRUCTION
};

#endif  //!__EREPLICAPACKETTYPE__H__
