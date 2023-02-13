#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	UpdateStripUiMessage(AMFArrayValue* arguments);
	const double GetYPosition() { return yPosition; };
	const double GetXPosition() { return xPosition; };
	const StripId GetStripID() { return stripID; };
	const BehaviorState GetStateID() { return stateID; };
	const uint32_t GetBehaviorID() { return behaviorID; };
private:
	double yPosition;
	double xPosition;
	StripId stripID;
	BehaviorState stateID;
	uint32_t behaviorID;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
