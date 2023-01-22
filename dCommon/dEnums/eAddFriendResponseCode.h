#pragma once

#ifndef __EADDFRIENDRESPONSECODE__H__
#define __EADDFRIENDRESPONSECODE__H__

#include <cstdint>

enum class eAddFriendResponseCode : uint8_t {
	ACCEPTED = 0,
	REJECTED,
	BUSY,
	CANCELLED
};

#endif  //!__ADDFRIENDRESPONSECODE__H__
