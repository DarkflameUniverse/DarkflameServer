#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RearrangeStripMessage : public BehaviorMessageBase {
public:
	RearrangeStripMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const uint32_t GetStripId() { return stripId; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	const BehaviorState GetStateID() { return stateID; };
private:
	uint32_t srcActionIndex;
	StripId stripId;
	uint32_t dstActionIndex;
	BehaviorState stateID;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
