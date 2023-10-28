#include "AddMessage.h"

AddMessage::AddMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	behaviorIndex = 0;
	auto* behaviorIndexValue = arguments->Get<double>("BehaviorIndex");

	if (!behaviorIndexValue) return;

	behaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LOG_DEBUG("behaviorId %i index %i", behaviorId, behaviorIndex);
}
