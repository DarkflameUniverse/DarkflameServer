#ifndef __ADDACTIONMESSAGE__H__
#define __ADDACTIONMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Send if a player takes an Action A from the toolbox and adds it to an already existing strip
 * 
 */
class AddActionMessage : public BehaviorMessageBase {
public:
	AddActionMessage(AMFArrayValue* arguments);
	int32_t GetActionIndex() const { return actionIndex; };
	Action GetAction() const { return action; };
	ActionContext GetActionContext() const { return actionContext; };
private:
	int32_t actionIndex = -1;
	ActionContext actionContext;
	Action action;
};

#endif  //!__ADDACTIONMESSAGE__H__
