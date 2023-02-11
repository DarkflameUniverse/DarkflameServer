#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RearrangeStripMessage : public BehaviorMessageBase {
public:
	RearrangeStripMessage(AMFArrayValue* arguments);
	uint32_t GetSrcActionIndex() { return srcActionIndex; };
	uint32_t GetStripID() { return stripID; };
	uint32_t GetBehaviorID() { return behaviorID; };
	uint32_t GetDstActionIndex() { return dstActionIndex; };
	BehaviorState GetStateID() { return stateID; };
private:
	uint32_t srcActionIndex;
	uint32_t stripID;
	uint32_t behaviorID;
	uint32_t dstActionIndex;
	BehaviorState stateID;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
