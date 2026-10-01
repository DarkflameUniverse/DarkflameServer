#ifndef BASERACINGCONTROLCOMPONENT_H
#define BASERACINGCONTROLCOMPONENT_H

#include "ActivityComponent.h"

/**
 * The base of the racing controls, as the client's LWOBaseRacingControlComponent: an activity component that racing
 * control (71) and gate rush control (103) are built on (its function table extends the activity component's).
 */
class BaseRacingControlComponent : public ActivityComponent {
protected:
	BaseRacingControlComponent(Entity* parent, const int32_t componentID) : ActivityComponent(parent, componentID) {}
};

#endif // BASERACINGCONTROLCOMPONENT_H
