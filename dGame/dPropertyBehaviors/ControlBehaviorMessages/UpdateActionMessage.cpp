#include "UpdateActionMessage.h"

#include "Action.h"

UpdateActionMessage::UpdateActionMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	auto* actionValue = arguments->GetArray("action");
	if (!actionValue) return;

	action = Action(actionValue);
	actionIndex = GetActionIndexFromArgument(arguments);

	LOG_DEBUG("type %s valueParameterName %s valueParameterString %s valueParameterDouble %f m_BehaviorId %i actionIndex %i stripId %i stateId %i", action.GetType().c_str(), action.GetValueParameterName().c_str(), action.GetValueParameterString().c_str(), action.GetValueParameterDouble(), m_BehaviorId, actionIndex, actionContext.GetStripId(), actionContext.GetStateId());
}
