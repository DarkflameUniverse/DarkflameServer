#include "SplitStripMessage.h"

SplitStripMessage::SplitStripMessage(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexValue) return;

	srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	srcStripId = GetStripIdFromArgument(arguments, "srcStripID");

	srcStateId = GetBehaviorStateFromArgument(arguments, "srcStateID");

	dstStripId = GetStripIdFromArgument(arguments, "dstStripID");

	dstStateId = GetBehaviorStateFromArgument(arguments, "dstStateID");

	auto* dstStripUiArray = arguments->FindValue<AMFArrayValue>("dstStripUI");
	if (!dstStripUiArray) return;

	auto* xPositionValue = dstStripUiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = dstStripUiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();
	xPosition = xPositionValue->GetDoubleValue();

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("SplitStripMessage", "behaviorId %i xPosition %f yPosition %f sourceStrip %i destinationStrip %i srcStt %i dstStt %i srcActindex %i", behaviorId, xPosition, yPosition, srcStripId, dstStripId, srcStateId, dstStateId, srcActionIndex);
}
