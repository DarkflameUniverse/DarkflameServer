#pragma once
#include "CppScripts.h"

class ActNinjaRebuild : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
private:
	LOT m_Base = 3086;
	Entity* m_BaseEntity = nullptr;
};

