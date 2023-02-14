#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MigrateActionsMessage : public BehaviorMessageBase {
public:
	MigrateActionsMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const StripId GetSrcStripID() { return srcStripID; };
	const BehaviorState GetSrcStateID() { return srcStateID; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	const StripId GetDstStripID() { return dstStripID; };
	const BehaviorState GetDstStateID() { return dstStateID; };
private:
	uint32_t srcActionIndex;
	StripId srcStripID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	BehaviorState dstStateID;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
