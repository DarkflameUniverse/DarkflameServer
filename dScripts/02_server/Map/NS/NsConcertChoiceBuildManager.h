#pragma once
#include "CppScripts.h"

struct Crate {
	std::string name;
	LOT lot;
	float time;
	std::string group;
};

class NsConcertChoiceBuildManager : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	static void SpawnCrate(Entity* self);
private:
	static const std::vector<Crate> crates;
};
