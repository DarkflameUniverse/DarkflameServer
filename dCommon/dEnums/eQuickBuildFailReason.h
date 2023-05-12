#ifndef __EQUICKBUILDFAILREASON__H__
#define __EQUICKBUILDFAILREASON__H__

#include <cstdint>

enum class eQuickBuildFailReason : uint32_t {
	NOT_GIVEN,
	OUT_OF_IMAGINATION,
	CANCELED_EARLY,
	BUILD_ENDED
};

#endif  //!__EQUICKBUILDFAILREASON__H__
