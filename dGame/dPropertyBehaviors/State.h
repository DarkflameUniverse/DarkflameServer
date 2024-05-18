#ifndef __STATE__H__
#define __STATE__H__

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

#endif  //!__STATE__H__
