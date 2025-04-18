#ifndef TRIALFACTIONARMORSERVER_
#define TRIALFACTIONARMORSERVER_

#include "CppScripts.h"

class TrialFactionArmorServer : public CppScripts::Script {
public:
	void OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) override;
};

#endif  //!TRIALFACTIONARMORSERVER_
