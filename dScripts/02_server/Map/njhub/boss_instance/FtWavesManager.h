#pragma once
#include "CppScripts.h"

class FtWavesManager : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void OnFireEventServerSide(
		Entity* self,
		Entity* sender,
		std::string args,
		int32_t param1,
		int32_t param2,
		int32_t param3
	) override;

	void ActivateWaveSpinners(Entity* self);

	void HandleSpinner(Entity* self, std::string spinner, std::string direction);

	void CheckWaveProgress(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    static Entity* engaged1;
    static Entity* engaged2;
    static Entity* engaged3;
    static Entity* engaged4;



	int bStarted;
	int LargeGroup;
	int WaveNum;

	int Wave1Progress;
	int Wave2Progress;
	int Wave3Progress;


	int HandSpinnerUp;
	int BlacksmithSpinnerUp;
	int OverseerSpinnerUp;
	int MarksmanSpinnerUp;
	int WolfSpinnerUp;
	int BeetleSpinnerUp;
	int ScientistSpinnerUp;
	int BonezaiSpinnerUp;

};
