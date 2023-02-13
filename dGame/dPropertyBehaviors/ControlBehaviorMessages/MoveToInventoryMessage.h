#ifndef __MOVETOINVENTORYMESSAGE__H__
#define __MOVETOINVENTORYMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

#pragma warning("This Control Behavior Message does not have a test yet. Non-developers can ignore this warning.")
class MoveToInventoryMessage: public BehaviorMessageBase {
public:
	MoveToInventoryMessage(AMFArrayValue* arguments);
	const uint32_t GetBehaviorID() { return behaviorID; };
	const uint32_t GetBehaviorIndex() { return behaviorIndex; };
private:
	uint32_t behaviorID;
	uint32_t behaviorIndex;
};

#endif  //!__MOVETOINVENTORYMESSAGE__H__
