#include "NsConcertChoiceBuildManager.h"
#include "EntityManager.h"

const std::vector<Crate> NsConcertChoiceBuildManager::crates{
		{ "laser", 11203, 5.0, "Concert_Laser_QB_" },
		{ "rocket", 11204, 3.0, "Concert_Rocket_QB_" },
		{ "speaker", 11205, 5.0, "Concert_Speaker_QB_" },
		{ "spotlight", 11206, 5.0, "Concert_Spotlight_QB_" }
};

void NsConcertChoiceBuildManager::OnStartup(Entity* self) {
	NsConcertChoiceBuildManager::SpawnCrate(self);
}

void NsConcertChoiceBuildManager::SpawnCrate(Entity* self) {
	const auto spawnNumber = self->GetVar<uint32_t>(u"spawnNumber") % crates.size();
	const auto crate = crates[spawnNumber];

	const auto groups = self->GetGroups();
	if (groups.empty())
		return;

	// Groups are of the form CB_1, CB_2, etc.
	auto group = groups.at(0);
	const auto splitGroup = GeneralUtils::SplitString(group, '_');
	if (splitGroup.size() < 2)
		return;
	const auto groupNumber = std::stoi(splitGroup.at(1));

	EntityInfo info{};
	info.lot = crate.lot;
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.spawnerID = self->GetObjectID();
	info.settings = {
		new LDFData<bool>(u"startsQBActivator", true),
		new LDFData<std::string>(u"grpNameQBShowBricks", crate.group + std::to_string(groupNumber)),
		new LDFData<std::u16string>(u"groupID", GeneralUtils::ASCIIToUTF16("Crate_" + group)),
		new LDFData<float>(u"crateTime", crate.time),
	};

	auto* spawnedCrate = EntityManager::Instance()->CreateEntity(info);
	EntityManager::Instance()->ConstructEntity(spawnedCrate);

	spawnedCrate->AddDieCallback([self]() {
		self->CancelAllTimers(); // Don't switch if the crate was smashed
		self->SetVar<LWOOBJID>(u"currentCrate", LWOOBJID_EMPTY);
		});

	self->SetVar<uint32_t>(u"spawnNumber", spawnNumber + 1);
	self->SetVar<float>(u"currentTimer", crate.time);
	self->SetVar<LWOOBJID>(u"currentCrate", spawnedCrate->GetObjectID());

	// Timer that rotates the crates
	self->AddCallbackTimer(crate.time, [self]() {
		auto crateID = self->GetVar<LWOOBJID>(u"currentCrate");
		if (crateID != LWOOBJID_EMPTY) {
			EntityManager::Instance()->DestroyEntity(crateID);
			self->SetVar<LWOOBJID>(u"currentCrate", LWOOBJID_EMPTY);
		}

		SpawnCrate(self);
		});
}
