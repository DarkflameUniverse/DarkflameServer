#ifndef __STATE__H__
#define __STATE__H__

#include "Strip.h"

class AMFArrayValue;

class State {
public:
	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const;

private:
	std::vector<Strip> m_Strips;
};

#endif  //!__STATE__H__
