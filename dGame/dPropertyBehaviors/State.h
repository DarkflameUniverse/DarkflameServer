#ifndef STATE_H
#define STATE_H

#include "Strip.h"

namespace tinyxml2 {
	class XMLElement;
}

class AMFArrayValue;

class State {
public:
	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const;

	void Serialize(tinyxml2::XMLElement& state) const;
	void Deserialize(const tinyxml2::XMLElement& state);
private:
	std::vector<Strip> m_Strips;
};

#endif  //!STATE_H
