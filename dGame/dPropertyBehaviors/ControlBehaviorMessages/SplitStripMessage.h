#ifndef __SPLITSTRIPMESSAGE__H__
#define __SPLITSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class SplitStripMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t srcActionIndex;
	StripId srcStripId;
	BehaviorState srcStateId;
	StripId dstStripId;
	BehaviorState dstStateId;
	double yPosition;
	double xPosition;
	uint32_t behaviorId;
};

#endif  //!__SPLITSTRIPMESSAGE__H__
