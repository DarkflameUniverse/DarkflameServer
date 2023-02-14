#include "RearrangeStripMessage.h"

RearrangeStripMessage::RearrangeStripMessage(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	stripId = GetStripIdFromArgument(arguments);

	behaviorId = GetBehaviorIDFromArgument(arguments);

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	stateID = GetBehaviorStateFromArgument(arguments);
	Game::logger->LogDebug("RearrangeStripMessage", "srcactionIndex %i dstactionIndex %i stripId %i behaviorId %i stateId %i", srcActionIndex, dstActionIndex, stripId, behaviorId, stateID);
}
