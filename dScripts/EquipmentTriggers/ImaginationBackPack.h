#ifndef IMAGINATIONBACKPACK_H
#define IMAGINATIONBACKPACK_H

#include "CppScripts.h"

class ImaginationBackPack : public CppScripts::Script {
public:
	void OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) override;
	void NotifyPlayerResurrectionFinished(Entity& self, GameMessages::PlayerResurrectionFinished& msg) override;
	void OnFactionTriggerItemUnequipped(Entity* itemOwner, LWOOBJID itemObjId) override;
};

#endif  //!IMAGINATIONBACKPACK_H
