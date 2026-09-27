#ifndef EUGCRESOURCETYPE_H
#define EUGCRESOURCETYPE_H

#include <cstdint>

// The kinds of file the client keeps for a blueprint (a player's model or a car or rocket), as it numbers them in
// REQUEST_UGC_MANIFEST_INFO and UGC_MANIFEST_RESPONSE and in its download paths
enum class eUgcResourceType : uint8_t {
	LXFML = 0,
	NIF,
	HKX,
	DDS
};

#endif  //!EUGCRESOURCETYPE_H
