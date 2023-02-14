#include "RenameMessage.h"

RenameMessage::RenameMessage(AMFArrayValue* arguments) {
	behaviorId = GetBehaviorIDFromArgument(arguments);

	auto* nameAmf = arguments->FindValue<AMFStringValue>("Name");
	if (!nameAmf) return;

	name = nameAmf->GetStringValue();
	Game::logger->LogDebug("RenameMessage", "behaviorId %i n %s", behaviorId, name.c_str());
}
