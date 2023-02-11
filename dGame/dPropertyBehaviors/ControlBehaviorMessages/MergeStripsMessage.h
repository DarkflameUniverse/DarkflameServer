#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MergeStripsMessage : public BehaviorMessageBase {
public:
	MergeStripsMessage(AMFArrayValue* arguments);
	StripId GetSrcStripID() { return srcStripID; };
	BehaviorState GetDstStateID() { return dstStateID; };
	BehaviorState GetSrcStateID() { return srcStateID; };
	uint32_t GetDstActionIndex() { return dstActionIndex; };
	StripId GetDstStripID() { return dstStripID; };
	uint32_t GetBehaviorID() { return behaviorID; };
private:
	StripId srcStripID;
	BehaviorState dstStateID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	uint32_t behaviorID;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
