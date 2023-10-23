#include "MigrateActionsMessage.h"

MigrateActionsMessage::MigrateActionsMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	sourceActionContext = ActionContext(arguments, "srcStateID", "srcStripID");
	srcActionIndex = GetActionIndexFromArgument(arguments, "srcActionIndex");

	destinationActionContext = ActionContext(arguments, "dstStateID", "dstStripID");
	dstActionIndex = GetActionIndexFromArgument(arguments, "dstActionIndex");

	LOG_DEBUG("srcactionIndex %i dstactionIndex %i srcstripId %i dststripId %i srcstateId %i dststateId %i behaviorId %i", srcActionIndex, dstActionIndex, sourceActionContext.GetStripId(), destinationActionContext.GetStripId(), sourceActionContext.GetStateId(), destinationActionContext.GetStateId(), behaviorId);
}
