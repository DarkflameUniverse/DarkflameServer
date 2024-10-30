#ifndef LUCGENERICINTERACT_H
#define LUCGENERICINTERACT_H

#include "CppScripts.h"

class LupGenericInteract : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};

#endif  //!LUCGENERICINTERACT_H

