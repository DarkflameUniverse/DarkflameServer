#include "MigrateActionsMessage.h"

MigrateActionsMessage::MigrateActionsMessage(AMFArrayValue* arguments) {
	auto* srcActionIndexAmf = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexAmf) return;

	srcActionIndex = static_cast<uint32_t>(srcActionIndexAmf->GetDoubleValue());

	srcStripID = GetStripIdFromArgument(arguments, "srcStripID");

	srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexAmf = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexAmf) return;

	dstActionIndex = static_cast<uint32_t>(dstActionIndexAmf->GetDoubleValue());

	dstStripID = GetStripIdFromArgument(arguments, "dstStripID");

	dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("MigrateActionsMessage", "srcactionIndex %i dstactionIndex %i srcstripId %i dststripId %i srcstateId %i dststateId %i behaviorId %i", srcActionIndex, dstActionIndex, srcStripID, dstStripID, srcStateID, dstStateID, behaviorId);
}
