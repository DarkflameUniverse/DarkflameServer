#ifndef __LUCGENERICINTERACT__H__
#define __LUCGENERICINTERACT__H__

#include "CppScripts.h"

class LupGenericInteract : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};

#endif  //!__LUCGENERICINTERACT__H__

