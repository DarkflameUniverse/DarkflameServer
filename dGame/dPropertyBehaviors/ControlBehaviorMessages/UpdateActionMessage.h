#ifndef __UPDATEACTIONMESSAGE__H__
#define __UPDATEACTIONMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player updates the value in an Action
 * 
 */
class UpdateActionMessage : public BehaviorMessageBase {
public:
	UpdateActionMessage(AMFArrayValue* arguments);
	const uint32_t GetActionIndex() { return actionIndex; };
	ActionContext GetActionContext() { return actionContext; };
	Action GetAction() { return action; };
private:
	uint32_t actionIndex;
	ActionContext actionContext;
	Action action;
}; 

#endif  //!__UPDATEACTIONMESSAGE__H__
