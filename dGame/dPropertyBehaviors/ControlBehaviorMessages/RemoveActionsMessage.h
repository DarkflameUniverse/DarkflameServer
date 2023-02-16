#ifndef __REMOVEACTIONSMESSAGE__H__
#define __REMOVEACTIONSMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player removes any Action after the first one from a Strip
 * 
 */
class RemoveActionsMessage : public BehaviorMessageBase {
public:
	RemoveActionsMessage(AMFArrayValue* arguments);
	const uint32_t GetActionIndex() { return actionIndex; };
	ActionContext GetActionContext() { return actionContext; };
private:
	ActionContext actionContext;
	uint32_t actionIndex;
};

#endif  //!__REMOVEACTIONSMESSAGE__H__
