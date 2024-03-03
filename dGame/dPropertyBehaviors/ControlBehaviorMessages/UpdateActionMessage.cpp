#include "UpdateActionMessage.h"

#include "Action.h"

UpdateActionMessage::UpdateActionMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionIndex{ GetActionIndexFromArgument(arguments) }
	, m_ActionContext{ arguments } {

	const auto* const actionValue = arguments.GetArray("action");
	if (!actionValue) return;
	
	m_Action = Action{ *actionValue };

	LOG_DEBUG("type %s valueParameterName %s valueParameterString %s valueParameterDouble %f behaviorId %i actionIndex %i stripId %i stateId %i", m_Action.GetType().c_str(), m_Action.GetValueParameterName().c_str(), m_Action.GetValueParameterString().c_str(), m_Action.GetValueParameterDouble(), m_BehaviorId, m_ActionIndex, m_ActionContext.GetStripId(), m_ActionContext.GetStateId());
}
