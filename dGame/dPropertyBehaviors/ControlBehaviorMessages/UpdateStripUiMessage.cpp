#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	position = StripUiPosition(arguments);
	actionContext = ActionContext(arguments);

	LogDebug("xPosition %f yPosition %f stripId %i stateId %i behaviorId %i", position.GetX(), position.GetY(), actionContext.GetStripId(), actionContext.GetStateId(), behaviorId);
}
