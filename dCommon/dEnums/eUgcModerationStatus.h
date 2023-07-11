
#ifndef __EUGCMODERATIONSTATUS__H__
#define __EUGCMODERATIONSTATUS__H__

#include <cstdint>

enum class eUgcModerationStatus : uint32_t {
	NoStatus,
	Approved,
	Rejected,
};

#endif  //!__EUGCMODERATIONSTATUS__H__
