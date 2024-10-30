#ifndef REMOVESTRIPMESSAGE_H
#define REMOVESTRIPMESSAGE_H

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player removes the first Action from a strip.
 * 
 */
class RemoveStripMessage : public BehaviorMessageBase {
public:
	RemoveStripMessage(const AMFArrayValue& arguments);
	
	const ActionContext& GetActionContext() const noexcept { return m_ActionContext; }

private:
	ActionContext m_ActionContext;
};

#endif  //!REMOVESTRIPMESSAGE_H
