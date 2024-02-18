#include "RenameMessage.h"

RenameMessage::RenameMessage(const AMFArrayValue* arguments) : BehaviorMessageBase{ arguments } {
	const auto* const nameAmf = arguments->Get<std::string>("Name");
	if (!nameAmf) return;

	m_Name = nameAmf->GetValue();
	LOG_DEBUG("behaviorId %i n %s", m_BehaviorId, m_Name.c_str());
}
