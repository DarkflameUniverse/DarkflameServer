#include "MergeStripsMessage.h"

MergeStripsMessage::MergeStripsMessage(AMFArrayValue* arguments) {
	srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexValue) return;

	dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	behaviorID = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("MergeStripsMessage", "srcStpId %i dstStpId %i srcSttId %i dstSttId %i dstAcnNdx %i bhId %i", srcStripID, dstStripID, srcStateID, dstStateID, dstActionIndex, behaviorID);
}

