#include "NsWinterRaceServer.h"
#include "GameMessages.h"
#include "RacingControlComponent.h"

using std::unique_ptr;
using std::make_unique;

void NsWinterRaceServer::OnStartup(Entity* self) {
	GameMessages::ConfigureRacingControl config;
	auto& raceSet = config.racingSettings;

	raceSet.Insert<std::u16string>("GameType", u"Racing");
	raceSet.Insert<std::u16string>("GameState", u"Starting");
	raceSet.Insert<int32_t>("Number_Of_PlayersPerTeam", 6);
	raceSet.Insert<int32_t>("Minimum_Players_to_Start", 2);
	raceSet.Insert<int32_t>("Minimum_Players_for_Group_Achievments", 2);

	raceSet.Insert<int32_t>("Car_Object", 7703);
	raceSet.Insert<std::u16string>("Race_PathName", u"MainPath");
	raceSet.Insert<int32_t>("Current_Lap", 1);
	raceSet.Insert<int32_t>("Number_of_Laps", 3);
	raceSet.Insert<int32_t>("activityID", 60);

	raceSet.Insert<int32_t>("Place_1", 100);
	raceSet.Insert<int32_t>("Place_2", 90);
	raceSet.Insert<int32_t>("Place_3", 80);
	raceSet.Insert<int32_t>("Place_4", 70);
	raceSet.Insert<int32_t>("Place_5", 60);
	raceSet.Insert<int32_t>("Place_6", 50);

	raceSet.Insert<int32_t>("Num_of_Players_1", 15);
	raceSet.Insert<int32_t>("Num_of_Players_2", 25);
	raceSet.Insert<int32_t>("Num_of_Players_3", 50);
	raceSet.Insert<int32_t>("Num_of_Players_4", 85);
	raceSet.Insert<int32_t>("Num_of_Players_5", 90);
	raceSet.Insert<int32_t>("Num_of_Players_6", 100);

	raceSet.Insert<int32_t>("Number_of_Spawn_Groups", 1);
	raceSet.Insert<int32_t>("Red_Spawners", 4847);
	raceSet.Insert<int32_t>("Blue_Spawners", 4848);
	raceSet.Insert<int32_t>("Blue_Flag", 4850);
	raceSet.Insert<int32_t>("Red_Flag", 4851);
	raceSet.Insert<int32_t>("Red_Point", 4846);
	raceSet.Insert<int32_t>("Blue_Point", 4845);
	raceSet.Insert<int32_t>("Red_Mark", 4844);
	raceSet.Insert<int32_t>("Blue_Mark", 4843);

	config.Send(self->GetObjectID());
}
