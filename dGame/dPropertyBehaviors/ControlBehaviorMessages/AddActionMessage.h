#ifndef __ADDACTIONMESSAGE__H__
#define __ADDACTIONMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddActionMessage : public BehaviorMessageBase {
public:
	AddActionMessage(AMFArrayValue* arguments);
	const uint32_t GetActionIndex() { return actionIndex; };
	Action GetAction() { return action; };
	ActionContext GetActionContext() { return actionContext; };
private:
	uint32_t actionIndex;
	ActionContext actionContext;
	Action action;
};

#endif  //!__ADDACTIONMESSAGE__H__
