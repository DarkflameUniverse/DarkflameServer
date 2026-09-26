#ifndef __STATE__H__
#define __STATE__H__

#include "Strip.h"

namespace tinyxml2 {
	class XMLElement;
}

class AMFArrayValue;
class ModelComponent;
struct UpdateResult;

class State {
public:
	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const;

	void Serialize(tinyxml2::XMLElement& state) const;
	void Deserialize(const tinyxml2::XMLElement& state);

	void Update(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult);

	void OnChatMessageReceived(const std::string& sMessage);
	void OnHit();
private:

	// The strips contained within this state.
	std::vector<Strip> m_Strips;
};

#endif  //!__STATE__H__
