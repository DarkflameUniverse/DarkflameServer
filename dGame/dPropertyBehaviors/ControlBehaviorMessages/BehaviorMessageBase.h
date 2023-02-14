#ifndef __BEHAVIORMESSAGEBASE__H__
#define __BEHAVIORMESSAGEBASE__H__

#include <stdexcept>
#include <string>

#include "AMFFormat.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

#include "Game.h"
#include "dLogger.h"

class BehaviorMessageBase {
public:
	uint32_t GetBehaviorIDFromArgument(AMFArrayValue* arguments, const std::string& key = "BehaviorID") {
		auto* behaviorIDValue = arguments->FindValue<AMFStringValue>(key);
		uint32_t behaviorID = -1;

		if (behaviorIDValue) {
			behaviorID = std::stoul(behaviorIDValue->GetStringValue());
		} else if (!arguments->FindValue<AMFUndefinedValue>(key)) {
			throw std::invalid_argument("Unable to find behavior ID from argument \"" + key + "\"");
		}

		return behaviorID;
	}

	BehaviorState GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key = "stateID") {
		auto* stateIDValue = arguments->FindValue<AMFDoubleValue>(key);
		if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

		return static_cast<BehaviorState>(stateIDValue->GetDoubleValue());
	}

	StripId GetStripIdFromArgument(AMFArrayValue* arguments, const std::string& key = "stripID") {
		auto* stripIdValue = arguments->FindValue<AMFDoubleValue>(key);
		if (!stripIdValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

		return static_cast<StripId>(stripIdValue->GetDoubleValue());
	}
	const uint32_t GetBehaviorId() { return behaviorId; };
protected:
	int32_t behaviorId = -1;
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
