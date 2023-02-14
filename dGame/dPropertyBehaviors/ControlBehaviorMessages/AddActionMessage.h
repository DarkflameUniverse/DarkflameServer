#ifndef __ADDACTIONMESSAGE__H__
#define __ADDACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddActionMessage : public BehaviorMessageBase {
public:
	AddActionMessage(AMFArrayValue* arguments);
	const uint32_t GetActionIndex() { return actionIndex; };
	const StripId GetStripId() { return stripId; };
	const BehaviorState GetStateId() { return stateId; };
	const std::string& GetType() { return type; };
	const std::string& GetValueParameterName() { return valueParameterName; };
	const std::string& GetValueParameterString() { return valueParameterString; };
	const double GetValueParameterDouble() { return valueParameterDouble; };
private:
	uint32_t actionIndex;
	StripId stripId;
	BehaviorState stateId;
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
};

#endif  //!__ADDACTIONMESSAGE__H__
