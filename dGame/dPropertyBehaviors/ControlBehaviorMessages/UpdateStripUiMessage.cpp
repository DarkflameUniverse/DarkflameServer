#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	position = StripUiPosition(arguments);
	actionContext = ActionContext(arguments);

	LOG_DEBUG("xPosition %f yPosition %f stripId %i stateId %i m_BehaviorId %i", position.GetX(), position.GetY(), actionContext.GetStripId(), actionContext.GetStateId(), m_BehaviorId);
}
