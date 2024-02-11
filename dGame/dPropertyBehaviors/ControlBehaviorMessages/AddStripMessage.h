#ifndef __ADDSTRIPMESSAGE__H__
#define __ADDSTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"
#include "StripUiPosition.h"

#include <vector>

class Action;
class AMFArrayValue;

/**
 * @brief Sent in 2 contexts:
 * A player adds an Action A from their toolbox without attaching it to an existing Strip.  In this case, only 1 action is sent.
 * A player moves a Strip from BehaviorState A directly to BehaviorState B.  In this case, a list of actions are sent. 
 * 
 */
class AddStripMessage : public BehaviorMessageBase {
public:
	AddStripMessage(AMFArrayValue* const arguments);
	StripUiPosition GetPosition() const { return position; };
	ActionContext GetActionContext() const { return actionContext; };
	std::vector<Action> GetActionsToAdd() const;
private:
	StripUiPosition position;
	ActionContext actionContext;
	std::vector<Action> actionsToAdd;
};

#endif  //!__ADDSTRIPMESSAGE__H__
