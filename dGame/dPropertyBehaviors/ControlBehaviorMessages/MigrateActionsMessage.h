#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MigrateActionsMessage : public BehaviorMessageBase {
public:
	MigrateActionsMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	ActionContext GetSourceActionContext() { return sourceActionContext; };
	ActionContext GetDestinationActionContext() { return destinationActionContext; };
private:
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	uint32_t srcActionIndex;
	uint32_t dstActionIndex;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
