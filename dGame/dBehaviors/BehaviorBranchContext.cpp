#include "BehaviorBranchContext.h"


BehaviorBranchContext::BehaviorBranchContext() {
	this->isProjectile = false;
}

BehaviorBranchContext::BehaviorBranchContext(const LWOOBJID target, const float duration, const NiPoint3& referencePosition) {
	this->target = target;
	this->duration = duration;
	this->referencePosition = referencePosition;
	this->isProjectile = false;
}
