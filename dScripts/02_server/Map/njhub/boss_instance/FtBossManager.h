#pragma once
#include "CppScripts.h"

class FtBossManager : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void PlayCelebration(Entity* self, int cutscene);

	void TeleportPlayers(Entity* self);

	void StartElement(Entity* self, std::string elementString);

	void TeleportPortal(Entity* self, Spawner* spawner, int nodeNum);

	void PlayerHitPortal(Entity* self, std::string element);

	void StartNewWave(Entity* self);

	void ActivateWaveSpinners(Entity* self);

	void SpinnerEnemies(Entity* self);

	void RunDieCallbacks(Entity* self);

	void StunEnemies(Entity* self, bool value);

	void CheckProgress(Entity* self);

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;

	void OnPlayerExit(Entity* self, Entity* player) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

	void SpawnMortar(Entity* self, const std::string& loc);

private:

    static Entity* player1;
    static Entity* player2;
    static Entity* player3;
    static Entity* player4;

    static Entity* randomPlayer;

	int randomPlayerNum = 0;

	int playerIndex = 0;

	int LargeTeam = 1;

    int PlayerCount = 0;
	int bStarted = 0;
	int nextNode = 0;

	int waveNum = 1;
	int wave1Progress = 0;	
	int wave2Progress = 0;	
	int wave3Progress = 0;	

	std::string CurrentElement = "nill";
	bool PortalAlreadyHitOnce;
	bool PortalHit;

	LWOOBJID GarmadonID;
	LWOOBJID TornadoID;

	bool pauseMortar = false;

	std::vector<std::string> elementalOrder = { "earth", "ice", "lightning", "fire" };
    std::map<std::string, int> Timers = { {"earth", 40}, {"ice", 30}, {"lightning", 40}, {"fire", 20} };
    std::map<std::string, int> RailNode = { {"earth", 14}, {"ice", 5}, {"lightning", 10}, {"fire", 3} };
    std::map<std::string, int> RailLength = { {"earth", 15}, {"ice", 6}, {"lightning", 21}, {"fire", 4} };
    std::string CutsceneVolume = "BossCutsceneVolume";
    std::string gameSpaceVolume = "BossGameSpace";
    std::string SpawnerSpinners = "BossSpawnerSpinners";
    std::string ElementalPortalSpawner = "BossPortalSpawn_";
    std::string LGTornadoSpawner = "BossLGTornado";
    std::string EndpostSpawner = "BossEndposts_";
    std::string ActivatorSpawner = "BossActivators_";
    std::string GarmadonSpawner = "BossLordGarmadon";
    std::string baseEnemiesSpawner = "BossSpawnerSpinner_";
    std::string BouncerSpawner = "BossEndBouncer";
    std::string LightningBouncerSpawner = "BossLightningBouncers";
    std::string WaveOverCine = "BossWaveOverCam";
    std::string SpawnBouncerCine = "BossEndBouncerSpawnCam";
    int DelayToCounterSpawn = 2;
    std::vector<int> DestroyPortalMissions = { 2094 };

    // Struct for wave enemy loadouts
    struct Wave {
        int LOT;
        int LargeNum;
        int SmallNum;
    };

    std::map<std::string, int> enemies = {
        {"blacksmith", 16836},
        {"overseer", 16847},
        {"marksman", 16849},
        {"wolf", 13996},
        {"beetle", 13998},
        {"vulture", 14000},
        {"hand", 14002},
        {"frakjaw", 16854},
        {"tornado", 16807},
        {"portal", 16802},
        {"garmadon", 16810}
    };

};
