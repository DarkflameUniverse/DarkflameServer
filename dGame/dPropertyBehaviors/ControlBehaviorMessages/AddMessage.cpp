#include "AddMessage.h"

AddMessage::AddMessage(const AMFArrayValue& arguments, const LWOOBJID _owningPlayerID) : m_OwningPlayerID{ _owningPlayerID }, BehaviorMessageBase { arguments } {
	const auto* const behaviorIndexValue = arguments.Get<double>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	m_BehaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetValue());
	LOG_DEBUG("behaviorId %llu index %i", m_BehaviorId, m_BehaviorIndex);
}
