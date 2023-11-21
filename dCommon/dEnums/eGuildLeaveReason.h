#pragma once
#include <cstdint>

#ifndef __EGUILDLEAVEREASON__H__
#define __EGUILDLEAVEREASON__H__

enum class eGuildLeaveReason : uint8_t {
	LEFT = 0,
	KICKED
};

#endif  //!__EGUILDLEAVEREASON__H__
