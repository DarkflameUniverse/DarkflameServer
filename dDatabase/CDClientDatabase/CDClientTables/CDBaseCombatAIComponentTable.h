#ifndef CDBASECOMBATAICOMPONENTTABLE_H
#define CDBASECOMBATAICOMPONENTTABLE_H

#include "CDTable.h"

struct CDBaseCombatAIComponent {
	int32_t id;
	float aggroRadius;
	float tetherSpeed;
	float pursuitSpeed;
	float softTetherRadius;
	float hardTetherRadius;
};

class CDBaseCombatAIComponentTable : public CDTable<CDBaseCombatAIComponentTable, std::vector<CDBaseCombatAIComponent>> {
public:
	void LoadValuesFromDatabase();
	void LoadValuesFromDefaults();

	std::vector<CDBaseCombatAIComponent> Query(std::function<bool(CDBaseCombatAIComponent)> predicate);
	const std::vector<CDBaseCombatAIComponent>& GetEntries() const;
};

#endif //CDBASECOMBATAICOMPONENTTABLE_H