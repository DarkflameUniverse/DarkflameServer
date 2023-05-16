#include "MoveToInventoryMessage.h"

MoveToInventoryMessage::MoveToInventoryMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	auto* behaviorIndexValue = arguments->Get<double>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	behaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LogDebug("behaviorId %i behaviorIndex %i", behaviorId, behaviorIndex);
}
