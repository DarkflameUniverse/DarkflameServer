#ifndef __BUBBLE_STATUE__H__
#define __BUBBLE_STATUE__H__

#include "CppScripts.h"

class BubbleStatue : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;
private:
	const float m_BubbleStatueRadius = 10.0;
	// No clue what "wszType" means but thats what the client calls it.
	const std::u16string wszType = u"skunk";
	const bool specialAnims = false;
};

#endif  //!__BUBBLE_STATUE__H__
