#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) {
	stripId = GetStripIDFromArgument(arguments);
	behaviorState = GetBehaviorStateFromArgument(arguments);
	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("RemoveStripMessage", "stpId %i bhStt %i bhId %i", stripId, behaviorState, behaviorId);
}
