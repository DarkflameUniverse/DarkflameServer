#ifndef __ACTION__H__
#define __ACTION__H__

#include <string>

class AMFArrayValue;

/**
 * @brief Sent if a ControlBehavior message has an Action associated with it
 * 
 */
class Action {
public:
	Action();
	Action(AMFArrayValue* arguments);
	const std::string& GetType() const { return type; };
	const std::string& GetValueParameterName() const { return valueParameterName; };
	const std::string& GetValueParameterString() const { return valueParameterString; };
	const double GetValueParameterDouble() const { return valueParameterDouble; };

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
private:
	std::string type;
	std::string valueParameterName;
	std::string valueParameterString;
	double valueParameterDouble;
};

#endif  //!__ACTION__H__
