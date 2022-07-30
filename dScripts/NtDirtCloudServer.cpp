#include "NtDirtCloudServer.h"
#include "MissionComponent.h"

std::map<std::string, std::vector<int32_t>> NtDirtCloudServer::m_Missions =
{
	{"Dirt_Clouds_Sent", {1333,1253}},
	{"Dirt_Clouds_Assem", {1333,1276}},
	{"Dirt_Clouds_Para", {1333,1277}},
	{"Dirt_Clouds_Halls", {1333,1283}}
};

void NtDirtCloudServer::OnStartup(Entity* self) {
	self->SetVar(u"CloudOn", true);
}

void NtDirtCloudServer::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "soapspray") {
		return;
	}

	if (!self->GetVar<bool>(u"CloudOn")) {
		return;
	}

	const auto mySpawner = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	if (m_Missions.count(mySpawner) == 0) {
		return;
	}

	const auto& myMis = m_Missions[mySpawner];

	auto* missionComponent = caster->GetComponent<MissionComponent>();

	if (missionComponent == nullptr) {
		return;
	}

	for (const auto missionID : myMis) {
		missionComponent->ForceProgressTaskType(missionID, 1, 1);
	}

	self->SetVar(u"CloudOn", false);

	self->Smash(self->GetObjectID(), VIOLENT);
}
