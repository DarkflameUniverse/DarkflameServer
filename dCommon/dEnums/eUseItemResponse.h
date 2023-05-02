#ifndef __EUSEITEMRESPONSE__H__
#define __EUSEITEMRESPONSE__H__

#include <cstdint>

enum class eUseItemResponse : uint32_t {
	NoImaginationForPet = 1,
	FailedPrecondition,
	MountsNotAllowed
};

#endif  //!__EUSEITEMRESPONSE__H__
