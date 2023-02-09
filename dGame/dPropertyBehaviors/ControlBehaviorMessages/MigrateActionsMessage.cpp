#include "MigrateActionsMessage.h"

void MigrateActionsMessage::Parse(AMFArrayValue* arguments) {
	auto* srcActionIndexAmf = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexAmf) return;

	srcActionIndex = static_cast<uint32_t>(srcActionIndexAmf->GetDoubleValue());

	srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexAmf = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexAmf) return;

	dstActionIndex = static_cast<uint32_t>(dstActionIndexAmf->GetDoubleValue());

	dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	behaviorID = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("MigrateActionsMessage", "srcAcnNdx %i dstAcnNdx %i srcStpId %i dstStpId %i srcSttId %i dstSttId %i bhid %i", srcActionIndex, dstActionIndex, srcStripID, dstStripID, srcStateID, dstStateID, behaviorID);
}
