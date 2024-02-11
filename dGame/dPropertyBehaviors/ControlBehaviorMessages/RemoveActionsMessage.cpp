#include "RemoveActionsMessage.h"

RemoveActionsMessage::RemoveActionsMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionIndex{ GetActionIndexFromArgument(arguments) }
	, m_ActionContext{ arguments } {

	LOG_DEBUG("behaviorId %i actionIndex %i stripId %i stateId %i", m_BehaviorId, m_ActionIndex, m_ActionContext.GetStripId(), m_ActionContext.GetStateId());
}
