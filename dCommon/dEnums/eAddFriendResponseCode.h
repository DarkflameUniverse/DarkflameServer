#pragma once

#ifndef EADDFRIENDRESPONSECODE_H
#define EADDFRIENDRESPONSECODE_H

#include <cstdint>

enum class eAddFriendResponseCode : uint8_t {
	ACCEPTED = 0,
	REJECTED,
	BUSY,
	CANCELLED
};

#endif  //!ADDFRIENDRESPONSECODE_H
