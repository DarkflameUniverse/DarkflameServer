#include "PetDigBuild.h"
#include "EntityManager.h"
#include "MissionComponent.h"

void PetDigBuild::OnRebuildComplete(Entity* self, Entity* target) {
	auto flagNumber = self->GetVar<std::u16string>(u"flagNum");

	EntityInfo info{};
	auto pos = self->GetPosition();
	pos.SetY(pos.GetY() + 0.5f);
	info.pos = pos;
	info.rot = self->GetRotation();
	info.spawnerID = self->GetSpawnerID();
	info.settings = {
			new LDFData<LWOOBJID>(u"builder", target->GetObjectID()),
			new LDFData<LWOOBJID>(u"X", self->GetObjectID())
	};

	if (!flagNumber.empty()) {
		info.lot = 7410; // Normal GF treasure
		info.settings.push_back(new LDFData<std::u16string>(u"groupID", u"Flag" + flagNumber));
	} else {
		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent != nullptr && missionComponent->GetMissionState(746) == MissionState::MISSION_STATE_ACTIVE) {
			info.lot = 9307; // Special Captain Jack treasure that drops a mission item
		} else {
			info.lot = 3495; // Normal AG treasure
		}
	}

	auto* treasure = EntityManager::Instance()->CreateEntity(info);
	EntityManager::Instance()->ConstructEntity(treasure);
	self->SetVar<LWOOBJID>(u"chestObj", treasure->GetObjectID());
}

void PetDigBuild::OnDie(Entity* self, Entity* killer) {
	auto treasureID = self->GetVar<LWOOBJID>(u"chestObj");
	if (treasureID == LWOOBJID_EMPTY)
		return;

	auto treasure = EntityManager::Instance()->GetEntity(treasureID);
	if (treasure == nullptr)
		return;

	// If the quick build expired and the treasure was not collected, hide the treasure
	if (!treasure->GetIsDead()) {
		treasure->Smash(self->GetObjectID(), SILENT);
	}
}
