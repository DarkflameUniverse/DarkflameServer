#include "AddStripMessage.h"

#include "Action.h"

AddStripMessage::AddStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);
	position = StripUiPosition(arguments);

	auto* strip = arguments->GetArray("strip");
	if (!strip) return;

	auto* actions = strip->GetArray("actions");
	if (!actions) return;

	for (uint32_t actionNumber = 0; actionNumber < actions->GetDense().size(); actionNumber++) {
		auto* actionValue = actions->GetArray(actionNumber);
		if (!actionValue) continue;

		actionsToAdd.push_back(Action(actionValue));

		LOG_DEBUG("xPosition %f yPosition %f stripId %i stateId %i behaviorId %i t %s valueParameterName %s valueParameterString %s valueParameterDouble %f", position.GetX(), position.GetY(), actionContext.GetStripId(), actionContext.GetStateId(), behaviorId, actionsToAdd.back().GetType().c_str(), actionsToAdd.back().GetValueParameterName().c_str(), actionsToAdd.back().GetValueParameterString().c_str(), actionsToAdd.back().GetValueParameterDouble());
	}
	LOG_DEBUG("number of actions %i", actionsToAdd.size());
}

std::vector<Action> AddStripMessage::GetActionsToAdd() {
	return actionsToAdd;
}
