#ifndef __NTNAOMIBREADCRUMBSERVER__H__
#define __NTNAOMIBREADCRUMBSERVER__H__

#include "CppScripts.h"

class NtNaomiBreadcrumbServer : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};

#endif  //!__NTNAOMIBREADCRUMBSERVER__H__
