#pragma once

#ifndef __ADDFRIENDRESPONSECODE__H__
#define __ADDFRIENDRESPONSECODE__H__

#include <cstdint>

enum class AddFriendResponseCode : uint8_t {
	ACCEPTED = 0,
	REJECTED,
	BUSY,
	CANCELLED
};

#endif  //!__ADDFRIENDRESPONSECODE__H__
