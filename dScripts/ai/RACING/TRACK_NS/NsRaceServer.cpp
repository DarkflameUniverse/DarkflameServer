#include "NsRaceServer.h"

#include "RacingControlComponent.h"
#include "Entity.h"

using std::unique_ptr;
using std::make_unique;

void NsRaceServer::OnStartup(Entity* self) {
	GameMessages::ConfigureRacingControl config;
	auto& raceSet = config.racingSettings;

	raceSet.push_back(make_unique<LDFData<std::u16string>>(u"GameType", u"Racing"));
	raceSet.push_back(make_unique<LDFData<std::u16string>>(u"GameState", u"Starting"));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Number_Of_PlayersPerTeam", 6));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Minimum_Players_to_Start", 2));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Minimum_Players_for_Group_Achievements", 2));

	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Car_Object", 7703));
	raceSet.push_back(make_unique<LDFData<std::u16string>>(u"Race_PathName", u"MainPath"));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Current_Lap", 1));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Number_of_Laps", 3));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"activityID", 42));

	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_1", 100));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_2", 90));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_3", 80));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_4", 70));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_5", 60));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Place_6", 50));

	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_1", 15));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_2", 25));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_3", 50));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_4", 85));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_5", 90));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Num_of_Players_6", 100));

	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Number_of_Spawn_Groups", 1));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Red_Spawners", 4847));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Blue_Spawners", 4848));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Blue_Flag", 4850));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Red_Flag", 4851));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Red_Point", 4846));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Blue_Point", 4845));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Red_Mark", 4844));
	raceSet.push_back(make_unique<LDFData<int32_t>>(u"Blue_Mark", 4843));

	std::vector<Entity*> racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	for (auto* const racingController : racingControllers) {
		auto* racingComponent = racingController->GetComponent<RacingControlComponent>();
		if (racingComponent) racingComponent->MsgConfigureRacingControl(config);
	}
}
