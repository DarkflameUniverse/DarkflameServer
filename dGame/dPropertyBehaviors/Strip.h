#ifndef __STRIP__H__
#define __STRIP__H__

#include "Action.h"
#include "StripUiPosition.h"

#include "DluAssert.h"

#include <vector>

namespace tinyxml2 {
	class XMLElement;
}

class AMFArrayValue;
class ModelComponent;

class Strip {
public:
	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const noexcept { return m_Actions.empty(); }

	void Serialize(tinyxml2::XMLElement& strip) const;
	void Deserialize(const tinyxml2::XMLElement& strip);

	const std::vector<Action>& GetActions() const { return m_Actions; }
	const Action& GetNextAction() const { DluAssert(m_NextActionIndex < m_Actions.size()); return m_Actions[m_NextActionIndex]; }

	void IncrementAction();
	void Spawn(LOT object, Entity& entity);
	void Update(float deltaTime, ModelComponent& modelComponent);
	void SpawnDrop(LOT dropLOT, Entity& entity);
	void ProcNormalAction(float deltaTime, ModelComponent& modelComponent);
private:
	float m_PausedTime{ 0.0f };
	size_t m_NextActionIndex{ 0 };
	std::vector<Action> m_Actions;
	StripUiPosition m_Position;
};

#endif  //!__STRIP__H__
