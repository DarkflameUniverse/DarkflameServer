#ifndef __ADDSTRIPMESSAGE__H__
#define __ADDSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddStripMessage : public BehaviorMessageBase {
public:
	AddStripMessage(AMFArrayValue* arguments);
	const StripId GetStripId() { return stripId; };
	const BehaviorState GetStateId() { return stateId; };
	const std::string& GetType() { return type; };
	const std::string& GetValueParameterName() { return valueParameterName; };
	const std::string& GetValueParameterString() { return valueParameterString; };
	const double GetXPosition() { return xPosition; };
	const double GetYPosition() { return yPosition; };
	const double GetValueParameterDouble() { return valueParameterDouble; };
private:
	double xPosition;
	double yPosition;
	StripId stripId;
	BehaviorState stateId;
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
};

#endif  //!__ADDSTRIPMESSAGE__H__
