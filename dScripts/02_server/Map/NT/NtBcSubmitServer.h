#ifndef __NTBCSUBMITSERVER__H__
#define __NTBCSUBMITSERVER__H__

#include "CppScripts.h"

class NtBcSubmitServer : public virtual CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};

#endif  //!__NTBCSUBMITSERVER__H__
