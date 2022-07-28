#pragma once
#include "CppScripts.h"

struct SpyDialogue {
	std::string token;
	uint32_t conversationID;
};

struct SpyData {
	uint32_t flagID;
	LOT itemID;
	uint32_t missionID;
};

class NtFactionSpyServer : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName, float_t pathTime, float_t totalTime, int32_t waypoint) override;
protected:
	virtual void SetVariables(Entity* self);
	bool IsSpy(Entity* self, Entity* possibleSpy);
	LWOOBJID ParamObjectForConversationID(Entity* self, uint32_t conversationID);

	const std::string m_ProximityName = "SpyDistance";
	const std::u16string m_SpyDialogueNotification = u"displayDialogueLine";
	const std::u16string m_SpyCinematicVariable = u"SpyCinematic";
	const std::u16string m_SpyCinematicObjectsVariable = u"SpyCinematicObjects";
	const std::u16string m_CinematicRootVariable = u"CinematicRoot";
	const std::u16string m_SpyProximityVariable = u"Proximity";
	const std::u16string m_SpyDialogueTableVariable = u"SpyDialogueTable";
	const std::u16string m_SpyDataVariable = u"SpyData";
};
