#ifndef NTBCSUBMITSERVER_H
#define NTBCSUBMITSERVER_H

#include "CppScripts.h"

class NtBcSubmitServer : public virtual CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};

#endif  //!NTBCSUBMITSERVER_H
