#ifndef ENEMYCLEARTHREAT_H
#define ENEMYCLEARTHREAT_H

#include "CppScripts.h"

class EnemyClearThreat : public CppScripts::Script {
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};

#endif  //!ENEMYCLEARTHREAT_H
