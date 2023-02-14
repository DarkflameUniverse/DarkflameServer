#include "AddActionMessage.h"

AddActionMessage::AddActionMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);
	actionIndex = GetActionIndexFromArgument(arguments);

	auto* actionValue = arguments->FindValue<AMFArrayValue>("action");
	if (!actionValue) return;

	action = Action(actionValue);

	Game::logger->LogDebug("AddActionMessage", "actionIndex %i stripId %i stateId %i type %s valueParameterName %s valueParameterString %s valueParameterDouble %f behaviorId %i", actionIndex, actionContext.GetStripId(), actionContext.GetStateId(), action.GetType().c_str(), action.GetValueParameterName().c_str(), action.GetValueParameterString().c_str(), action.GetValueParameterDouble(), behaviorId);
}
