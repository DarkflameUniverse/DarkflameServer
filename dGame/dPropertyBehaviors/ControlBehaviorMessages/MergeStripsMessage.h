#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player adds the first Action of Strip A to a Strip B
 * 
 */
class MergeStripsMessage : public BehaviorMessageBase {
public:
	MergeStripsMessage(AMFArrayValue* arguments);
	int32_t GetDstActionIndex() { return dstActionIndex; };
	ActionContext GetSourceActionContext() { return sourceActionContext; };
	ActionContext GetDestinationActionContext() { return destinationActionContext; };
	const std::vector<Action>& GetMigratedActions() { return migratedActions; };
	void SetMigratedActions(std::vector<Action>::const_iterator start, std::vector<Action>::const_iterator end) { migratedActions.assign(start, end); };
private:
	std::vector<Action> migratedActions;
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	int32_t dstActionIndex;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
