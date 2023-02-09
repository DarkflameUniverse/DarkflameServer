#ifndef __ADDSTRIPMESSAGE__H__
#define __ADDSTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddStripMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
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
