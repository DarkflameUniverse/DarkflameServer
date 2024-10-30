#ifndef EUSEITEMRESPONSE_H
#define EUSEITEMRESPONSE_H

#include <cstdint>

enum class eUseItemResponse : uint32_t {
	NoImaginationForPet = 1,
	FailedPrecondition,
	MountsNotAllowed
};

#endif  //!EUSEITEMRESPONSE_H
