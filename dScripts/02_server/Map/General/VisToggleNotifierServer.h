#ifndef VISTOGGLENOTIFIERSERVER_H
#define VISTOGGLENOTIFIERSERVER_H

#include "CppScripts.h"

class VisToggleNotifierServer : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
protected:
	void SetGameVariables(std::map<int32_t, std::string>& gameVariables) { m_GameVariables = gameVariables; }
private:
	std::map<int32_t, std::string> m_GameVariables;
};

#endif  //!VISTOGGLENOTIFIERSERVER_H
