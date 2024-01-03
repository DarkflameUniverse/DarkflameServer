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
	int32_t GetActionIndex() const { return actionIndex; };
	ActionContext GetActionContext() const { return actionContext; };
	Action GetAction() const { return action; };
private:
	int32_t actionIndex;
	ActionContext actionContext;
	Action action;
}; 

#endif  //!__UPDATEACTIONMESSAGE__H__
