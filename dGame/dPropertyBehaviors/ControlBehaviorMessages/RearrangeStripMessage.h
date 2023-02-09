#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RearrangeStripMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t srcActionIndex;
	uint32_t stripID;
	uint32_t behaviorID;
	uint32_t dstActionIndex;
	BehaviorState stateID;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
