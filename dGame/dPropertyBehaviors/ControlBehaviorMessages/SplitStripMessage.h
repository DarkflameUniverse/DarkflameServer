#ifndef __SPLITSTRIPMESSAGE__H__
#define __SPLITSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class SplitStripMessage : public BehaviorMessageBase {
public:
	SplitStripMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const StripId GetSrcStripId() { return srcStripId; };
	const BehaviorState GetSrcStateId() { return srcStateId; };
	const StripId GetDstStripId() { return dstStripId; };
	const BehaviorState GetDstStateId() { return dstStateId; };
	const double GetYPosition() { return yPosition; };
	const double GetXPosition() { return xPosition; };
private:
	uint32_t srcActionIndex;
	StripId srcStripId;
	BehaviorState srcStateId;
	StripId dstStripId;
	BehaviorState dstStateId;
	double yPosition;
	double xPosition;
};

#endif  //!__SPLITSTRIPMESSAGE__H__
