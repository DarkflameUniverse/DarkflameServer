#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player moves an Action after the first Action to a different Strip
 * 
 */
class MigrateActionsMessage : public BehaviorMessageBase {
public:
	MigrateActionsMessage(AMFArrayValue* arguments);
	int32_t GetSrcActionIndex() const { return srcActionIndex; };
	int32_t GetDstActionIndex() const { return dstActionIndex; };
	ActionContext GetSourceActionContext() const { return sourceActionContext; };
	ActionContext GetDestinationActionContext() const { return destinationActionContext; };
	const std::vector<Action>& GetMigratedActions() const { return migratedActions; };
	void SetMigratedActions(std::vector<Action>::const_iterator start, std::vector<Action>::const_iterator end) { migratedActions.assign(start, end); };
private:
	std::vector<Action> migratedActions;
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	int32_t srcActionIndex;
	int32_t dstActionIndex;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
