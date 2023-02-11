#ifndef __REMOVESTRIPMESSAGE__H__
#define __REMOVESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RemoveStripMessage : public BehaviorMessageBase {
public:
	RemoveStripMessage(AMFArrayValue* arguments);
	StripId GetStripId() { return stripId; };
	BehaviorState GetBehaviorState() { return behaviorState; };
	uint32_t GetBehaviorId() { return behaviorId; };
private:
	StripId stripId;
	BehaviorState behaviorState;
	uint32_t behaviorId;
};

#endif  //!__REMOVESTRIPMESSAGE__H__
