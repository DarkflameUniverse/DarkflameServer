#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	double yPosition;
	double xPosition;
	StripId stripID;
	BehaviorState stateID;
	uint32_t behaviorID;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
