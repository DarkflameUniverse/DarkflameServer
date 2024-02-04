#include "AddMessage.h"

AddMessage::AddMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	m_BehaviorIndex = 0;
	auto* behaviorIndexValue = arguments->Get<double>("BehaviorIndex");

	if (!behaviorIndexValue) return;

	m_BehaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LOG_DEBUG("m_BehaviorId %i index %i", m_BehaviorId, m_BehaviorIndex);
}
