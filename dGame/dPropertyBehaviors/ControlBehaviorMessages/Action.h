#ifndef __ACTION__H__
#define __ACTION__H__

#include <string>

namespace tinyxml2 {
	class XMLElement;
};

class AMFArrayValue;

/**
 * @brief Sent if a ControlBehavior message has an Action associated with it
 * 
 */
class Action {
public:
	Action() = default;
	Action(const AMFArrayValue& arguments);
	[[nodiscard]] std::string_view GetType() const { return m_Type; };
	[[nodiscard]] std::string_view GetValueParameterName() const { return m_ValueParameterName; };
	[[nodiscard]] std::string_view GetValueParameterString() const { return m_ValueParameterString; };
	[[nodiscard]] double GetValueParameterDouble() const noexcept { return m_ValueParameterDouble; };

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;

	void Serialize(tinyxml2::XMLElement& action) const;
	void Deserialize(const tinyxml2::XMLElement& action);
private:
	double m_ValueParameterDouble{ 0.0 };
	std::string m_Type{ "" };
	std::string m_ValueParameterName{ "" };
	std::string m_ValueParameterString{ "" };
};

#endif  //!__ACTION__H__
