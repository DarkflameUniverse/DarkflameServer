#ifndef __STRIP__H__
#define __STRIP__H__

#include "Action.h"
#include "StripUiPosition.h"

#include <vector>

namespace tinyxml2 {
	class XMLElement;
}

class AMFArrayValue;
class ModelComponent;

class Strip {
public:
	void Update(float deltaTime, const ModelComponent& modelComponent);

	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const noexcept { return m_Actions.empty(); }

	void Serialize(tinyxml2::XMLElement& strip) const;
	void Deserialize(const tinyxml2::XMLElement& strip);
private:
	uint32_t m_ActionIndex{ 0 };

	std::vector<Action> m_Actions;
	StripUiPosition m_Position;
};

#endif  //!__STRIP__H__
