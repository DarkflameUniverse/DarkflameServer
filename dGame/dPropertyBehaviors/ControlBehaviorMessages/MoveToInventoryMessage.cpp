#include "MoveToInventoryMessage.h"

MoveToInventoryMessage::MoveToInventoryMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	auto* behaviorIndexValue = arguments->Get<double>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	m_BehaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LOG_DEBUG("m_BehaviorId %i m_BehaviorIndex %i", m_BehaviorId, m_BehaviorIndex);
}
