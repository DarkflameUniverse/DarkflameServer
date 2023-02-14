#include "MergeStripsMessage.h"

MergeStripsMessage::MergeStripsMessage(AMFArrayValue* arguments) {
	srcStripID = GetStripIdFromArgument(arguments, "srcStripID");

	dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexValue) return;

	dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	dstStripID = GetStripIdFromArgument(arguments, "dstStripID");

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("MergeStripsMessage", "srcstripId %i dststripId %i srcstateId %i dststateId %i dstactionIndex %i behaviorId %i", srcStripID, dstStripID, srcStateID, dstStateID, dstActionIndex, behaviorId);
}

