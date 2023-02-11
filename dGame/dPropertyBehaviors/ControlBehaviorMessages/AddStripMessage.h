#ifndef __ADDSTRIPMESSAGE__H__
#define __ADDSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddStripMessage : public BehaviorMessageBase {
public:
	AddStripMessage(AMFArrayValue* arguments);
	StripId GetStripId() { return stripId; };
	BehaviorState GetStateId() { return stateId; };
	std::string& GetType() { return type; };
	std::string& GetValueParameterName() { return valueParameterName; };
	std::string& GetValueParameterString() { return valueParameterString; };
	double GetXPosition() { return xPosition; };
	double GetYPosition() { return yPosition; };
	double GetValueParameterDouble() { return valueParameterDouble; };
	uint32_t GetBehaviorId() { return behaviorId; };
private:
	double xPosition;
	double yPosition;
	StripId stripId;
	BehaviorState stateId;
	uint32_t behaviorId;
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
};

#endif  //!__ADDSTRIPMESSAGE__H__
