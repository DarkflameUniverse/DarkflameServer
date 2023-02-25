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
	const uint32_t GetActionIndex() { return actionIndex; };
	Action GetAction() { return action; };
	ActionContext GetActionContext() { return actionContext; };
private:
	uint32_t actionIndex;
	ActionContext actionContext;
	Action action;
};

#endif  //!__ADDACTIONMESSAGE__H__
