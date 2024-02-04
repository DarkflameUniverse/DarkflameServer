#include "RenameMessage.h"

RenameMessage::RenameMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	auto* nameAmf = arguments->Get<std::string>("Name");
	if (!nameAmf) return;

	name = nameAmf->GetValue();
	LOG_DEBUG("m_BehaviorId %i n %s", m_BehaviorId, name.c_str());
}
