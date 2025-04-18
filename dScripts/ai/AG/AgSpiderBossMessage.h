#ifndef AGSPIDERBOSSMESSAGE_H
#define AGSPIDERBOSSMESSAGE_H

#include "CppScripts.h"

struct Box {
	LWOOBJID boxTarget{};
	bool isDisplayed{};
	bool isTouch{};
	bool isFirst{};
	LWOOBJID boxSelf{};
	std::u16string boxText{};
	int32_t boxTime{ 1 };

	void Reset() {
		boxTarget = LWOOBJID_EMPTY;
		isDisplayed = false;
		isTouch = false;
		isFirst = false;
		boxSelf = LWOOBJID_EMPTY;
		boxText.clear();
		boxTime = 1;
	}
};

class AgSpiderBossMessage : public CppScripts::Script {
public:
	Box GetBox(Entity* self) const;
	void SetBox(Entity* self, const Box& box) const;
	void MakeBox(Entity* self) const;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnOffCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void ResetBox(Entity* self) const;
};

#endif  //!AGSPIDERBOSSMESSAGE_H
