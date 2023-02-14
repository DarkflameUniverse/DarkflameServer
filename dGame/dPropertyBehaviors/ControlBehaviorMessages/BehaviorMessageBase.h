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
		} else if (arguments->FindValue<AMFUndefinedValue>(key) == nullptr) {
			throw std::invalid_argument("Unable to find behavior ID from argument \"" + key + "\"");
		}

		return behaviorID;
	}

	BehaviorState GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key = "stateID") {
		auto* stateIDValue = arguments->FindValue<AMFDoubleValue>(key);
		if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

		BehaviorState stateID = static_cast<BehaviorState>(stateIDValue->GetDoubleValue());

		return stateID;
	}

	StripId GetStripIDFromArgument(AMFArrayValue* arguments, const std::string& key = "stripID") {
		auto* stripIDValue = arguments->FindValue<AMFDoubleValue>(key);
		if (!stripIDValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

		StripId stripID = static_cast<StripId>(stripIDValue->GetDoubleValue());

		return stripID;
	}
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
