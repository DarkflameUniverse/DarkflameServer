#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	UpdateStripUiMessage(AMFArrayValue* arguments);
	const double GetYPosition() { return yPosition; };
	const double GetXPosition() { return xPosition; };
	const StripId GetStripId() { return stripId; };
	const BehaviorState GetStateID() { return stateID; };
private:
	double yPosition;
	double xPosition;
	StripId stripId;
	BehaviorState stateID;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
