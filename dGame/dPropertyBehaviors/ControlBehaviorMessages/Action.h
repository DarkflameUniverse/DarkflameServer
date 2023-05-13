#ifndef __ACTION__H__
#define __ACTION__H__

#include "BehaviorMessageBase.h"

/**
 * @brief Sent if a ControlBehavior message has an Action associated with it
 * 
 */
class Action {
public:
	Action();
	Action(AMFArrayValue* arguments);
	const std::string& GetType() { return type; };
	const std::string& GetValueParameterName() { return valueParameterName; };
	const std::string& GetValueParameterString() { return valueParameterString; };
	const double GetValueParameterDouble() { return valueParameterDouble; };
private:
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
};

#endif  //!__ACTION__H__
