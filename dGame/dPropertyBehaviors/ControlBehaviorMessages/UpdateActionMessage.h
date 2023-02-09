#ifndef __UPDATEACTIONMESSAGE__H__
#define __UPDATEACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class UpdateActionMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
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
