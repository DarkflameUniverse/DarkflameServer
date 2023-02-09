#ifndef __REMOVEACTIONSMESSAGE__H__
#define __REMOVEACTIONSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class RemoveActionsMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t behaviorID;
	uint32_t actionIndex;
	StripId stripID;
	BehaviorState stateID;
};

#endif  //!__REMOVEACTIONSMESSAGE__H__
