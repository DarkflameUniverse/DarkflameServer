#include "RenameMessage.h"

RenameMessage::RenameMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	auto* nameAmf = arguments->Get<std::string>("Name");
	if (!nameAmf) return;

	name = nameAmf->GetValue();
	LOG_DEBUG("behaviorId %i n %s", behaviorId, name.c_str());
}
