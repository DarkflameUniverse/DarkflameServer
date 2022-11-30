#pragma once

#include "dCommonVars.h"
#include "NiPoint3.h"

struct BehaviorBranchContext
{
	LWOOBJID target = LWOOBJID_EMPTY;

	float duration = 0;

	NiPoint3 referencePosition = {};

	bool isProjectile = false;

	uint32_t start = 0;

	BehaviorBranchContext();

	BehaviorBranchContext(LWOOBJID target, float duration = 0, const NiPoint3& referencePosition = NiPoint3(0, 0, 0));
};
