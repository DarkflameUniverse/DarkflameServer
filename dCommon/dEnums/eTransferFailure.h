#ifndef __ETRANSFERFAILURE__H__
#define __ETRANSFERFAILURE__H__

#include <cstdint>

/**
 * Why a transfer to a world got no world to go to. A TRANSFER_TO_WORLD message with an empty address
 * carries it in place of the port, and the client tells the player.
 */
enum class eTransferFailure : uint16_t {
	NoServer = 0,     // nothing is free to start the zone on
	MapLoad,          // the zone can not be loaded
	InstanceFull,     // the instance asked for is full
	Other             // every other value is a failure without a reason
};

#endif  //!__ETRANSFERFAILURE__H__
