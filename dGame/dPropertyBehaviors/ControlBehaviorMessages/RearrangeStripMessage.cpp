#include "RearrangeStripMessage.h"

void RearrangeStripMessage::Parse(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	stripID = GetStripIDFromArgument(arguments);

	behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	stateID = GetBehaviorStateFromArgument(arguments);
	Game::logger->LogDebug("RearrangeStripMessage", "srcAcnNdx %i dstAcnNdx %i stpId %i bhId %i sttId %i", srcActionIndex, dstActionIndex, stripID, behaviorID, stateID);
}
