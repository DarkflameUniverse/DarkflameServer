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
	AddActionMessage(const AMFArrayValue* arguments);

	[[nodiscard]] int32_t GetActionIndex() const noexcept { return m_ActionIndex; };

	[[nodiscard]] const Action& GetAction() const noexcept { return m_Action; };

	[[nodiscard]] const ActionContext& GetActionContext() const noexcept { return m_ActionContext; };

private:
	int32_t m_ActionIndex{ -1 };
	ActionContext m_ActionContext;
	Action m_Action;
};

#endif  //!__ADDACTIONMESSAGE__H__
