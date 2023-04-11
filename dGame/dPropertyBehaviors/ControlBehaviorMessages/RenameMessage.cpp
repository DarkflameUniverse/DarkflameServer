#include "RenameMessage.h"

RenameMessage::RenameMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	auto* nameAmf = arguments->FindValue<AMFStringValue>("Name");
	if (!nameAmf) return;

	name = nameAmf->GetStringValue();
	Game::logger->LogDebug("RenameMessage", "behaviorId %i n %s", behaviorId, name.c_str());
}
