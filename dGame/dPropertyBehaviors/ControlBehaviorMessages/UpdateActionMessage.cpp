#include "UpdateActionMessage.h"

#include "Action.h"

UpdateActionMessage::UpdateActionMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	auto* actionValue = arguments->FindValue<AMFArrayValue>("action");
	if (!actionValue) return;

	action = Action(actionValue);
	actionIndex = GetActionIndexFromArgument(arguments);

	Game::logger->LogDebug("UpdateActionMessage", "type %s valueParameterName %s valueParameterString %s valueParameterDouble %f behaviorId %i actionIndex %i stripId %i stateId %i", action.GetType().c_str(), action.GetValueParameterName().c_str(), action.GetValueParameterString().c_str(), action.GetValueParameterDouble(), behaviorId, actionIndex, actionContext.GetStripId(), actionContext.GetStateId());
}
