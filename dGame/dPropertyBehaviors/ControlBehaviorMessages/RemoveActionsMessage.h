#ifndef __REMOVEACTIONSMESSAGE__H__
#define __REMOVEACTIONSMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player removes any Action after the first one from a Strip
 * 
 */
class RemoveActionsMessage : public BehaviorMessageBase {
public:
	RemoveActionsMessage(const AMFArrayValue& arguments);

	[[nodiscard]] int32_t GetActionIndex() const noexcept { return m_ActionIndex; }

	[[nodiscard]] const ActionContext& GetActionContext() const noexcept { return m_ActionContext; }

private:
	int32_t m_ActionIndex;
	ActionContext m_ActionContext;
};

#endif  //!__REMOVEACTIONSMESSAGE__H__
