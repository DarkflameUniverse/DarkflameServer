#include "SplitStripMessage.h"

void SplitStripMessage::Parse(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexValue) return;

	srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	srcStripId = GetStripIDFromArgument(arguments, "srcStripID");

	srcStateId = GetBehaviorStateFromArgument(arguments, "srcStateID");

	dstStripId = GetStripIDFromArgument(arguments, "dstStripID");

	dstStateId = GetBehaviorStateFromArgument(arguments, "dstStateID");

	auto* dstStripUiArray = arguments->FindValue<AMFArrayValue>("dstStripUI");
	if (!dstStripUiArray) return;

	auto* xPositionValue = dstStripUiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = dstStripUiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();
	xPosition = xPositionValue->GetDoubleValue();

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("SplitStripMessage", "bhid %i x %f y %f srcStp %i dstStp %i srcStt %i dstStt %i srcActNdx %i", behaviorId, xPosition, yPosition, srcStripId, dstStripId, srcStateId, dstStateId, srcActionIndex);
}
