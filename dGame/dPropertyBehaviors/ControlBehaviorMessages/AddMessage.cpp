#include "AddMessage.h"

AddMessage::AddMessage(const AMFArrayValue& arguments) : BehaviorMessageBase{ arguments } {
	const auto* const behaviorIndexValue = arguments.Get<double>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	m_BehaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LOG_DEBUG("behaviorId %i index %i", m_BehaviorId, m_BehaviorIndex);
}
