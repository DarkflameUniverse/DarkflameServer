#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MergeStripsMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	StripId srcStripID;
	BehaviorState dstStateID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	uint32_t behaviorID;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
