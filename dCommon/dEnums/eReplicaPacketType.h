#ifndef EREPLICAPACKETTYPE_H
#define EREPLICAPACKETTYPE_H

#include <cstdint>

enum class eReplicaPacketType : uint8_t {
	CONSTRUCTION,
	SERIALIZATION,
	DESTRUCTION
};

#endif  //!EREPLICAPACKETTYPE_H
