#include "AddMessage.h"

AddMessage::AddMessage(AMFArrayValue* arguments) {
	behaviorId = GetBehaviorIDFromArgument(arguments);

	behaviorIndex = 0;
	auto* behaviorIndexValue = arguments->FindValue<AMFDoubleValue>("BehaviorIndex");

	if (!behaviorIndexValue) return;

	behaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetDoubleValue());
	Game::logger->LogDebug("AddMessage", "bhId %i ndx %i", behaviorId, behaviorIndex);
}
