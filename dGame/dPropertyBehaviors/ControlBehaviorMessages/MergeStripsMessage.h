#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MergeStripsMessage : public BehaviorMessageBase {
public:
	MergeStripsMessage(AMFArrayValue* arguments);
	const StripId GetSrcStripID() { return srcStripID; };
	const BehaviorState GetDstStateID() { return dstStateID; };
	const BehaviorState GetSrcStateID() { return srcStateID; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	const StripId GetDstStripID() { return dstStripID; };
	const uint32_t GetBehaviorID() { return behaviorID; };
private:
	StripId srcStripID;
	BehaviorState dstStateID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	uint32_t behaviorID;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
