#ifndef NTNAOMIBREADCRUMBSERVER_H
#define NTNAOMIBREADCRUMBSERVER_H

#include "CppScripts.h"

class NtNaomiBreadcrumbServer : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};

#endif  //!NTNAOMIBREADCRUMBSERVER_H
