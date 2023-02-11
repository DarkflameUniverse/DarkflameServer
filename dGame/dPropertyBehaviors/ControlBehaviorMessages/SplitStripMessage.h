#ifndef __SPLITSTRIPMESSAGE__H__
#define __SPLITSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class SplitStripMessage : public BehaviorMessageBase {
public:
	SplitStripMessage(AMFArrayValue* arguments);
	uint32_t GetSrcActionIndex() { return srcActionIndex; };
	StripId GetSrcStripId() { return srcStripId; };
	BehaviorState GetSrcStateId() { return srcStateId; };
	StripId GetDstStripId() { return dstStripId; };
	BehaviorState GetDstStateId() { return dstStateId; };
	double GetYPosition() { return yPosition; };
	double GetXPosition() { return xPosition; };
	uint32_t GetBehaviorId() { return behaviorId; };
private:
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
