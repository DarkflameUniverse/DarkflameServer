#ifndef __UPDATEACTIONMESSAGE__H__
#define __UPDATEACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateActionMessage : public BehaviorMessageBase {
public:
	UpdateActionMessage(AMFArrayValue* arguments);
	std::string& GetType() { return type; };
	std::string& GetValueParameterName() { return valueParameterName; };
	std::string& GetValueParameterString() { return valueParameterString; };
	double GetValueParameterDouble() { return valueParameterDouble; };
	uint32_t GetBehaviorID() { return behaviorID; };
	uint32_t GetActionIndex() { return actionIndex; };
	StripId GetStripID() { return stripID; };
	BehaviorState GetStateID() { return stateID; };
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
