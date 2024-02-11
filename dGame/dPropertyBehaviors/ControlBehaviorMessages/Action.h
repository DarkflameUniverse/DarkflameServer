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
	Action() = default;
	Action(const AMFArrayValue* arguments);
	[[nodiscard]] const std::string& GetType() const { return m_Type; };
	[[nodiscard]] const std::string& GetValueParameterName() const { return m_ValueParameterName; };
	[[nodiscard]] const std::string& GetValueParameterString() const { return m_ValueParameterString; };
	[[nodiscard]] double GetValueParameterDouble() const noexcept { return m_ValueParameterDouble; };

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;

private:
	double m_ValueParameterDouble{ 0.0 };
	std::string m_Type{ "" };
	std::string m_ValueParameterName{ "" };
	std::string m_ValueParameterString{ "" };
};

#endif  //!__ACTION__H__
