#ifndef UPDATEACTIONMESSAGE_H
#define UPDATEACTIONMESSAGE_H

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
	UpdateActionMessage(const AMFArrayValue& arguments);

	[[nodiscard]] int32_t GetActionIndex() const noexcept { return m_ActionIndex; }

	[[nodiscard]] const ActionContext& GetActionContext() const noexcept { return m_ActionContext; }

	[[nodiscard]] const Action& GetAction() const noexcept { return m_Action; }

private:
	int32_t m_ActionIndex;
	ActionContext m_ActionContext;
	Action m_Action;
}; 

#endif  //!UPDATEACTIONMESSAGE_H
