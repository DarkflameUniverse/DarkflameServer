#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	UpdateStripUiMessage(AMFArrayValue* arguments);
	double GetYPosition() { return yPosition; };
	double GetXPosition() { return xPosition; };
	StripId GetStripID() { return stripID; };
	BehaviorState GetStateID() { return stateID; };
	uint32_t GetBehaviorID() { return behaviorID; };
private:
	double yPosition;
	double xPosition;
	StripId stripID;
	BehaviorState stateID;
	uint32_t behaviorID;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
