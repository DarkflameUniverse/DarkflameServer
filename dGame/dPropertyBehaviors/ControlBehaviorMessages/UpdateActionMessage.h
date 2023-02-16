#ifndef __UPDATEACTIONMESSAGE__H__
#define __UPDATEACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateActionMessage : public BehaviorMessageBase {
public:
	UpdateActionMessage(AMFArrayValue* arguments);
	const std::string& GetType() { return type; };
	const std::string& GetValueParameterName() { return valueParameterName; };
	const std::string& GetValueParameterString() { return valueParameterString; };
	const double GetValueParameterDouble() { return valueParameterDouble; };
	const uint32_t GetBehaviorID() { return behaviorID; };
	const uint32_t GetActionIndex() { return actionIndex; };
	const StripId GetStripID() { return stripID; };
	const BehaviorState GetStateID() { return stateID; };
private:
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
	uint32_t behaviorID;
	uint32_t actionIndex;
	StripId stripID;
	BehaviorState stateID;
};

#endif  //!__UPDATEACTIONMESSAGE__H__
