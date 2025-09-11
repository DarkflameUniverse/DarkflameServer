#ifndef CDBASECOMBATAICOMPONENTTABLE_H
#define CDBASECOMBATAICOMPONENTTABLE_H

#include "CDTable.h"

struct CDBaseCombatAIComponent {
	int32_t id;
	int32_t behaviorType;
	float combatRoundLength;
	int32_t combatRole;
	float minRoundLength;
	float maxRoundLength;
	float tetherSpeed;
	float pursuitSpeed;
	float combatStartDelay;
	float softTetherRadius;
	float hardTetherRadius;
	float spawnTimer;
	int32_t tetherEffectID;
	bool ignoreMediator;
	float aggroRadius;
	bool ignoreStatReset;
	bool ignoreParent;
};

class CDBaseCombatAIComponentTable : public CDTable<CDBaseCombatAIComponentTable, std::vector<CDBaseCombatAIComponent>> {
public:
	void LoadValuesFromDatabase();
	void LoadValuesFromDefaults();

	std::vector<CDBaseCombatAIComponent> Query(std::function<bool(CDBaseCombatAIComponent)> predicate);
	const std::vector<CDBaseCombatAIComponent>& GetEntries() const;
};

#endif //CDBASECOMBATAICOMPONENTTABLE_H