#include "MergeStripsMessage.h"

MergeStripsMessage::MergeStripsMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	sourceActionContext = ActionContext(arguments, "srcStateID", "srcStripID");

	destinationActionContext = ActionContext(arguments, "dstStateID", "dstStripID");
	dstActionIndex = GetActionIndexFromArgument(arguments, "dstActionIndex");

	LOG_DEBUG("srcstripId %i dststripId %i srcstateId %i dststateId %i dstactionIndex %i behaviorId %i", sourceActionContext.GetStripId(), destinationActionContext.GetStripId(), sourceActionContext.GetStateId(), destinationActionContext.GetStateId(), dstActionIndex, behaviorId);
}

