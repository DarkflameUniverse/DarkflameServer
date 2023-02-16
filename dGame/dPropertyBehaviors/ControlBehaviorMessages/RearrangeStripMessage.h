#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RearrangeStripMessage : public BehaviorMessageBase {
public:
	RearrangeStripMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const uint32_t GetStripID() { return stripID; };
	const uint32_t GetBehaviorID() { return behaviorID; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	const BehaviorState GetStateID() { return stateID; };
private:
	uint32_t srcActionIndex;
	uint32_t stripID;
	uint32_t behaviorID;
	uint32_t dstActionIndex;
	BehaviorState stateID;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
