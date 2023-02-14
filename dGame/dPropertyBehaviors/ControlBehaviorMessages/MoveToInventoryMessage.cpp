#include "MoveToInventoryMessage.h"

MoveToInventoryMessage::MoveToInventoryMessage(AMFArrayValue* arguments) {
	behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* behaviorIndexValue = arguments->FindValue<AMFDoubleValue>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	behaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetDoubleValue());
	Game::logger->LogDebug("MoveToInventoryMessage", "bhId %i bhNdx %i", behaviorID, behaviorIndex);
}
