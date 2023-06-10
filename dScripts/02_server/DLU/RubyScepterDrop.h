#include "CppScripts.h"

#include "dCommonVars.h"

class Entity;

class RubyScepterDrop : public CppScripts::Script {
public:
	void OnDie(Entity* self, Entity* killer) override;
private:
	const LOT scepter = 38054;
};
