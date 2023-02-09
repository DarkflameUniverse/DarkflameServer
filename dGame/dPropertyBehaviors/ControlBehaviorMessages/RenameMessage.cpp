#include "RenameMessage.h"

void RenameMessage::Parse(AMFArrayValue* arguments) {
	behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* nameAmf = arguments->FindValue<AMFStringValue>("Name");
	if (!nameAmf) return;

	name = nameAmf->GetStringValue();
	Game::logger->LogDebug("RenameMessage", "bhId %i n %s", behaviorID, name.c_str());
}
