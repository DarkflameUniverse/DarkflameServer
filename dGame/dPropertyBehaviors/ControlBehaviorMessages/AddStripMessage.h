#ifndef __ADDSTRIPMESSAGE__H__
#define __ADDSTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"
#include "StripUiPosition.h"

#include <vector>

class Action;
class AMFArrayValue;

class AddStripMessage : public BehaviorMessageBase {
public:
	AddStripMessage(AMFArrayValue* arguments);
	StripUiPosition GetPosition() { return position; };
	ActionContext GetActionContext() { return actionContext; };
	std::vector<Action> GetActionsToAdd() { return actionsToAdd; };
private:
	StripUiPosition position;
	ActionContext actionContext;
	std::vector<Action> actionsToAdd;
};

#endif  //!__ADDSTRIPMESSAGE__H__
