#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) {
	stripId = GetStripIdFromArgument(arguments);
	behaviorState = GetBehaviorStateFromArgument(arguments);
	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("RemoveStripMessage", "stripId %i bhStt %i behaviorId %i", stripId, behaviorState, behaviorId);
}
