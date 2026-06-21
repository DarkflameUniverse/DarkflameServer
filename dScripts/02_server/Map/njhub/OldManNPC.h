#ifndef OLDMANNPC_H
#define OLDMANNPC_H

#include "CppScripts.h"

class OldManNPC : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
};

#endif  //!OLDMANNPC_H
