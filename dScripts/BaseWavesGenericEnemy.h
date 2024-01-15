#pragma once
#include "CppScripts.h"

class BaseWavesGenericEnemy : virtual public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
protected:
	virtual uint32_t GetPoints() { return 0; };
};
