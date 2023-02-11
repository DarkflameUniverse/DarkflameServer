#ifndef __ADDACTIONMESSAGE__H__
#define __ADDACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddActionMessage : public BehaviorMessageBase {
public:
	AddActionMessage(AMFArrayValue* arguments);
	uint32_t GetActionIndex() { return actionIndex; };
	StripId GetStripId() { return stripId; };
	BehaviorState GetStateId() { return stateId; };
	std::string& GetType() { return type; };
	std::string& GetValueParameterName() { return valueParameterName; };
	std::string& GetValueParameterString() { return valueParameterString; };
	double GetValueParameterDouble() { return valueParameterDouble; };
	uint32_t GetBehaviorId() { return behaviorId; };
private:
	uint32_t actionIndex;
	StripId stripId;
	BehaviorState stateId;
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
	uint32_t behaviorId;
};

#endif  //!__ADDACTIONMESSAGE__H__
