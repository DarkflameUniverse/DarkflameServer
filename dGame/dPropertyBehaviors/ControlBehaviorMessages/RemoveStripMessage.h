#ifndef __REMOVESTRIPMESSAGE__H__
#define __REMOVESTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player removes the first Action from a strip.
 * 
 */
class RemoveStripMessage : public BehaviorMessageBase {
public:
	RemoveStripMessage(AMFArrayValue* arguments);
	ActionContext GetActionContext() const { return actionContext; };
private:
	ActionContext actionContext;
};

#endif  //!__REMOVESTRIPMESSAGE__H__
