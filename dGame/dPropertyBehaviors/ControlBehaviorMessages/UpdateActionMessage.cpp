#include "UpdateActionMessage.h"

#include "Action.h"

UpdateActionMessage::UpdateActionMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionIndex{ GetActionIndexFromArgument(arguments) }
	, m_ActionContext{ arguments } {

	const auto* const actionValue = arguments.GetArray("action");
	if (!actionValue) return;
	
	m_Action = Action{ *actionValue };

	Log::Debug("type {:s} valueParameterName {:s} valueParameterString {:s} valueParameterDouble {:f} behaviorId {:d} actionIndex {:d} stripId {:d} stateId {:d}", m_Action.GetType(), m_Action.GetValueParameterName(), m_Action.GetValueParameterString(), m_Action.GetValueParameterDouble(), m_BehaviorId, m_ActionIndex, m_ActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()));
}
