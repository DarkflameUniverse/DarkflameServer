#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MigrateActionsMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t srcActionIndex;
	StripId srcStripID;
	BehaviorState srcStateID;
	uint32_t dstActionIndex;
	StripId dstStripID;
	BehaviorState dstStateID;
	uint32_t behaviorID;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
