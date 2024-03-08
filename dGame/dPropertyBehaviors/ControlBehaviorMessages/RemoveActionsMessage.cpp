#include "RemoveActionsMessage.h"

RemoveActionsMessage::RemoveActionsMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionIndex{ GetActionIndexFromArgument(arguments) }
	, m_ActionContext{ arguments } {

	Log::Debug("behaviorId {:d} actionIndex {:d} stripId {:d} stateId {:d}", m_BehaviorId, m_ActionIndex, m_ActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()));
}
