#ifndef __EMODERATIONSTATUS__H__
#define __EMODERATIONSTATUS__H__

#pragma once

#include <cstdint>

enum class ModerationStatus : uint32_t {
	NoStatus = 0,
	Approved,
	Rejected
};

#endif  //!__EMODERATIONSTATUS__H__
