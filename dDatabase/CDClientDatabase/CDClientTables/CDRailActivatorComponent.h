#pragma once
#include "CDTable.h"

struct CDRailActivatorComponent {
	int32_t id;
	std::u16string startAnimation;
	std::u16string loopAnimation;
	std::u16string stopAnimation;
	std::u16string startSound;
	std::u16string loopSound;
	std::u16string stopSound;
	std::pair<uint32_t, std::u16string> startEffectID;
	std::pair<uint32_t, std::u16string> loopEffectID;
	std::pair<uint32_t, std::u16string> stopEffectID;
	std::string preconditions;
	bool playerCollision;
	bool cameraLocked;
	bool damageImmune;
	bool noAggro;
	bool showNameBillboard;
};

class CDRailActivatorComponentTable : public CDTable<CDRailActivatorComponentTable, std::vector<CDRailActivatorComponent>> {
public:
	void LoadValuesFromDatabase();
	[[nodiscard]] CDRailActivatorComponent GetEntryByID(int32_t id) const;
private:
	static std::pair<uint32_t, std::u16string> EffectPairFromString(std::string& str);
};
