#ifndef __REMOVESTRIPMESSAGE__H__
#define __REMOVESTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class RemoveStripMessage : public BehaviorMessageBase {
public:
	RemoveStripMessage(AMFArrayValue* arguments);
	ActionContext GetActionContext() { return actionContext; };
private:
	ActionContext actionContext;
};

#endif  //!__REMOVESTRIPMESSAGE__H__
