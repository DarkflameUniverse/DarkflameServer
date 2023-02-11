#ifndef __REMOVEACTIONSMESSAGE__H__
#define __REMOVEACTIONSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class RemoveActionsMessage : public BehaviorMessageBase {
public:
	RemoveActionsMessage(AMFArrayValue* arguments);
	uint32_t GetBehaviorID() { return behaviorID; };
	uint32_t GetActionIndex() { return actionIndex; };
	StripId GetStripID() { return stripID; };
	BehaviorState GetStateID() { return stateID; };
private:
	uint32_t behaviorID;
	uint32_t actionIndex;
	StripId stripID;
	BehaviorState stateID;
};

#endif  //!__REMOVEACTIONSMESSAGE__H__
