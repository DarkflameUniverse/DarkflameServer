#ifndef __ADDACTIONMESSAGE__H__
#define __ADDACTIONMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class AddActionMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
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
