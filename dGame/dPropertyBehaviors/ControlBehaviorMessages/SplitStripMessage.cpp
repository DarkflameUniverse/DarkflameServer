#include "SplitStripMessage.h"

SplitStripMessage::SplitStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	sourceActionContext = ActionContext(arguments, "srcStateID", "srcStripID");
	srcActionIndex = GetActionIndexFromArgument(arguments, "srcActionIndex");

	destinationActionContext = ActionContext(arguments, "dstStateID", "dstStripID");
	destinationPosition = StripUiPosition(arguments, "dstStripUI");

	LOG_DEBUG("m_BehaviorId %i xPosition %f yPosition %f sourceStrip %i destinationStrip %i sourceState %i destinationState %i srcActindex %i", m_BehaviorId, destinationPosition.GetX(), destinationPosition.GetY(), sourceActionContext.GetStripId(), destinationActionContext.GetStripId(), sourceActionContext.GetStateId(), destinationActionContext.GetStateId(), srcActionIndex);
}
