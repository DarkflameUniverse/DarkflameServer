#include "NsRaceServer.h"

#include "RacingControlComponent.h"
#include "Entity.h"

void NsRaceServer::OnStartup(Entity* self) {
	std::vector<LDFBaseData*> settings = {
		new LDFData<std::u16string>(u"GameType", u"Racing"),
		new LDFData<std::u16string>(u"GameState", u"Starting"),
		new LDFData<int32_t>(u"Number_Of_PlayersPerTeam", 6),
		new LDFData<int32_t>(u"Minimum_Players_to_Start", 2),
		new LDFData<int32_t>(u"Minimum_Players_for_Group_Achievements", 2),

		new LDFData<int32_t>(u"Car_Object", 7703),
		new LDFData<std::u16string>(u"Race_PathName", u"MainPath"),
		new LDFData<int32_t>(u"Current_Lap", 1),
		new LDFData<int32_t>(u"Number_of_Laps", 3),
		new LDFData<int32_t>(u"activityID", 42),

		new LDFData<int32_t>(u"Place_1", 100),
		new LDFData<int32_t>(u"Place_2", 90),
		new LDFData<int32_t>(u"Place_3", 80),
		new LDFData<int32_t>(u"Place_4", 70),
		new LDFData<int32_t>(u"Place_5", 60),
		new LDFData<int32_t>(u"Place_6", 50),

		new LDFData<int32_t>(u"Num_of_Players_1", 15),
		new LDFData<int32_t>(u"Num_of_Players_2", 25),
		new LDFData<int32_t>(u"Num_of_Players_3", 50),
		new LDFData<int32_t>(u"Num_of_Players_4", 85),
		new LDFData<int32_t>(u"Num_of_Players_5", 90),
		new LDFData<int32_t>(u"Num_of_Players_6", 100),

		new LDFData<int32_t>(u"Number_of_Spawn_Groups", 1),
		new LDFData<int32_t>(u"Red_Spawners", 4847),
		new LDFData<int32_t>(u"Blue_Spawners", 4848),
		new LDFData<int32_t>(u"Blue_Flag", 4850),
		new LDFData<int32_t>(u"Red_Flag", 4851),
		new LDFData<int32_t>(u"Red_Point", 4846),
		new LDFData<int32_t>(u"Blue_Point", 4845),
		new LDFData<int32_t>(u"Red_Mark", 4844),
		new LDFData<int32_t>(u"Blue_Mark", 4843),
	};

	GameMessages::ConfigureRacingControl config;
	config.racingSettings = settings;
	std::vector<Entity*> racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	for (auto* const racingController : racingControllers) {
		auto* racingComponent = racingController->GetComponent<RacingControlComponent>();
		if (racingComponent) racingComponent->MsgConfigureRacingControl(config);
	}

	for (auto* const config : settings) {
		delete config;
	}
}
