#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MigrateActionsMessage : public BehaviorMessageBase {
public:
	MigrateActionsMessage(AMFArrayValue* arguments);
	uint32_t GetSrcActionIndex() { return srcActionIndex; };
	StripId GetSrcStripID() { return srcStripID; };
	BehaviorState GetSrcStateID() { return srcStateID; };
	uint32_t GetDstActionIndex() { return dstActionIndex; };
	StripId GetDstStripID() { return dstStripID; };
	BehaviorState GetDstStateID() { return dstStateID; };
	uint32_t GetBehaviorID() { return behaviorID; };
private:
	uint32_t srcActionIndex;
	StripId srcStripID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	BehaviorState dstStateID;
	uint32_t behaviorID;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
