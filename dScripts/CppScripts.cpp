//I can feel my soul being torn apart with every script added to this monstrosity.
// skate fast eat trash
// do you think god stays in heaven because he too lives in fear of what he's created?

#include "CppScripts.h"
#include "GameMessages.h"
#include "Entity.h"
#include "ScriptComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "InvalidScript.h"

//VE / AG scripts:
#include "AgShipPlayerDeathTrigger.h"
#include "AgShipPlayerShockServer.h"
#include "AgSpaceStuff.h"
#include "AgImagSmashable.h"
#include "NpcNpSpacemanBob.h"
#include "StoryBoxInteractServer.h"
#include "Binoculars.h"
#include "AllCrateChicken.h"
#include "SsModularBuildServer.h"
#include "ZoneAgProperty.h"
#include "TouchMissionUpdateServer.h"
#include "ActSharkPlayerDeathTrigger.h"
#include "BaseEnemyMech.h"
#include "ZoneAgSurvival.h"
#include "AgBusDoor.h"
#include "MaestromExtracticatorServer.h"
#include "NpcWispServer.h"
#include "AgCagedBricksServer.h"
#include "NpcEpsilonServer.h"
#include "AgTurret.h"
#include "AgMonumentBirds.h"
#include "AgLaserSensorServer.h"
#include "AgMonumentLaserServer.h"
#include "AgFans.h"
#include "RemoveRentalGear.h"
#include "NpcNjAssistantServer.h"
#include "EnemySpiderSpawner.h"
#include "AgQbElevator.h"
#include "AgPropGuard.h"
#include "AgBugsprayer.h"
#include "WishingWellServer.h"
#include "NpcAgCourseStarter.h"
#include "AgMonumentRaceGoal.h"
#include "ZoneAgSpiderQueen.h"
#include "SpiderBossTreasureChestServer.h"
#include "NpcCowboyServer.h"
#include "ZoneAgMedProperty.h"
#include "AgStromlingProperty.h"
#include "AgDarkSpiderling.h"
#include "PropertyFXDamage.h"
#include "AgPropguards.h"
#include "NpcPirateServer.h"
#include "AgPicnicBlanket.h"
#include "AgMonumentRaceCancel.h"
#include "VeBricksampleServer.h"
#include "PropertyBankInteract.h"
#include "VeMech.h"
#include "VeMissionConsole.h"
#include "VeEpsilonServer.h"
#include "AgSurvivalBuffStation.h"
#include "QbSpawner.h"
#include "AgQbWall.h"

// NS Scripts
#include "NsModularBuild.h"
#include "NsGetFactionMissionServer.h"
#include "NsQbImaginationStatue.h"
#include "NsConcertChoiceBuildManager.h"
#include "NsConcertChoiceBuild.h"
#include "NsConcertQuickBuild.h"
#include "AgStagePlatforms.h"
#include "NsConcertInstrument.h"
#include "NsJohnnyMissionServer.h"
#include "StinkyFishTarget.h"
#include "ZoneNsProperty.h"
#include "ZoneNsMedProperty.h"
#include "NsTokenConsoleServer.h"
#include "NsLupTeleport.h"
#include "ZoneNsWaves.h"
#include "WaveBossHammerling.h"
#include "WaveBossApe.h"
#include "WaveBossSpiderling.h"
#include "WaveBossHorsemen.h"
#include "MinigameTreasureChestServer.h"
#include "NsLegoClubDoor.h"
#include "ClRing.h"
#include "WildAmbients.h"
#include "PropertyDeathPlane.h"

// PR Scripts
#include "PrWhistle.h"
#include "PrSeagullFly.h"
#include "HydrantBroken.h"
#include "HydrantSmashable.h"
#include "PetDigServer.h"
#include "PetFromDigServer.h"
#include "PetFromObjectServer.h"
#include "DamagingPets.h"
#include "SpawnGryphonServer.h"

// GF Scripts
#include "GfTikiTorch.h"
#include "GfCampfire.h"
#include "GfOrgan.h"
#include "GfBanana.h"
#include "GfBananaCluster.h"
#include "GfJailkeepMission.h"
#include "TriggerAmbush.h"
#include "GfCaptainsCannon.h"
#include "MastTeleport.h"
#include "GfJailWalls.h"
#include "QbEnemyStunner.h"
#include "PetDigBuild.h"
#include "SpawnLionServer.h"
#include "BaseEnemyApe.h"
#include "GfApeSmashingQB.h"
#include "ZoneGfProperty.h"
#include "GfArchway.h"
#include "GfMaelstromGeyser.h"
#include "PirateRep.h"
#include "GfParrotCrash.h"

// SG Scripts
#include "SGCannon.h"
#include "ZoneSGServer.h"

// FV Scripts
#include "FvCandle.h"
#include "EnemyRoninSpawner.h"
#include "ActNinjaTurret.h"
#include "FvMaelstromCavalry.h"
#include "FvHorsemenTrigger.h"
#include "FvFlyingCreviceDragon.h"
#include "FvMaelstromDragon.h"
#include "FvDragonSmashingGolemQb.h"
#include "TreasureChestDragonServer.h"
#include "InstanceExitTransferPlayerToLastNonInstance.h"
#include "FvFreeGfNinjas.h"
#include "FvPandaServer.h"
#include "FvPandaSpawnerServer.h"
#include "ZoneFvProperty.h"
#include "FvBrickPuzzleServer.h"
#include "FvConsoleLeftQuickbuild.h"
#include "FvConsoleRightQuickbuild.h"
#include "FvFacilityBrick.h"
#include "FvFacilityPipes.h"
#include "ImgBrickConsoleQB.h"
#include "ActParadoxPipeFix.h"
#include "FvNinjaGuard.h"
#include "FvPassThroughWall.h"
#include "FvBounceOverWall.h"
#include "FvFong.h"
#include "FvMaelstromGeyser.h"

// FB Scripts
#include "AgJetEffectServer.h"
#include "AgSalutingNpcs.h"
#include "BossSpiderQueenEnemyServer.h"
#include "RockHydrantSmashable.h"
#include "SpecialImaginePowerupSpawner.h"

// Misc Scripts
#include "ExplodingAsset.h"
#include "ActPlayerDeathTrigger.h"
#include "CrabServer.h"
#include "GrowingFlower.h"
#include "BaseFootRaceManager.h"
#include "PropertyPlatform.h"
#include "MailBoxServer.h"
#include "ActMine.h"
#include "FireFirstSkillonStartup.h"

// Racing Scripts
#include "RaceImagineCrateServer.h"
#include "ActVehicleDeathTrigger.h"
#include "RaceImaginePowerup.h"
#include "RaceMaelstromGeiser.h"
#include "FvRaceSmashEggImagineServer.h"
#include "RaceSmashServer.h"

// NT Scripts
#include "NtSentinelWalkwayServer.h"
#include "NtParadoxTeleServer.h"
#include "NtDarkitectRevealServer.h"
#include "BankInteractServer.h"
#include "NtVentureSpeedPadServer.h"
#include "NtVentureCannonServer.h"
#include "NtCombatChallengeServer.h"
#include "NtCombatChallengeDummy.h"
#include "NtCombatChallengeExplodingDummy.h"
#include "BaseInteractDropLootServer.h"
#include "NtAssemblyTubeServer.h"
#include "NtParadoxPanelServer.h"
#include "TokenConsoleServer.h"
#include "NtImagBeamBuffer.h"
#include "NtBeamImaginationCollectors.h"
#include "NtDirtCloudServer.h"
#include "NtConsoleTeleportServer.h"
#include "SpawnShrakeServer.h"
#include "SpawnSaberCatServer.h"
#include "SpawnStegoServer.h"
#include "NtDukeServer.h"
#include "NtHaelServer.h"
#include "NtOverbuildServer.h"
#include "NtVandaServer.h"
#include "ForceVolumeServer.h"
#include "NtXRayServer.h"
#include "NtSleepingGuard.h"
#include "NtImagimeterVisibility.h"

// DLU Scripts
#include "DLUVanityNPC.h"

// AM Scripts
#include "AmConsoleTeleportServer.h"
#include "RandomSpawnerFin.h"
#include "RandomSpawnerPit.h"
#include "RandomSpawnerStr.h"
#include "RandomSpawnerZip.h"
#include "AmDarklingMech.h"
#include "AmBridge.h"
#include "AmDrawBridge.h"
#include "AmShieldGenerator.h"
#include "AmShieldGeneratorQuickbuild.h"
#include "AmDropshipComputer.h"
#include "AmScrollReaderServer.h"
#include "AmTemplateSkillVolume.h"
#include "EnemyNjBuff.h"
#include "AmSkeletonEngineer.h"
#include "AmSkullkinDrill.h"
#include "AmSkullkinDrillStand.h"
#include "AmSkullkinTower.h"
#include "AmDarklingDragon.h"
#include "AmBlueX.h"
#include "AmTeapotServer.h"

// NJ Scripts
#include "NjGarmadonCelebration.h"
#include "NjWuNPC.h"
#include "NjScrollChestServer.h"
#include "EnemySkeletonSpawner.h"
#include "NjRailSwitch.h"
#include "NjRailPostServer.h"
#include "NjRailActivatorsServer.h"
#include "NjColeNPC.h"
#include "NjNPCMissionSpinjitzuServer.h"
#include "NjJayMissionItems.h"
#include "FallingTile.h"
#include "ImaginationShrineServer.h"
#include "Lieutenant.h"
#include "RainOfArrows.h"
#include "NjIceRailActivator.h"
#include "CavePrisonCage.h"
#include "NjMonastryBossInstance.h"
#include "CatapultBouncerServer.h"
#include "CatapultBaseServer.h"
#include "NjhubLavaPlayerDeathTrigger.h"
#include "MonCoreNookDoors.h"
#include "MonCoreSmashableDoors.h"
#include "FlameJetServer.h"
#include "BurningTile.h"
#include "NjEarthDragonPetServer.h"
#include "NjEarthPetServer.h"
#include "NjDragonEmblemChestServer.h"
#include "NjNyaMissionitems.h"

// Scripted equipment
#include "AnvilOfArmor.h"
#include "CauldronOfLife.h"
#include "FountainOfImagination.h"
#include "Sunflower.h"
#include "BootyDigServer.h"
#include "PersonalFortress.h"
#include "PropertyDevice.h"
#include "ImaginationBackpackHealServer.h"
#include "LegoDieRoll.h"
#include "BuccaneerValiantShip.h"

// Survival scripts
#include "AgSurvivalStromling.h"
#include "AgSurvivalMech.h"
#include "AgSurvivalSpiderling.h"

// Frostburgh Scripts
#include "RockHydrantBroken.h"
#include "WhFans.h"

// WBL scripts
#include "WblGenericZone.h"

//Big bad global bc this is a namespace and not a class:
InvalidScript* invalidToReturn = new InvalidScript();
std::map<std::string, CppScripts::Script*> m_Scripts;

// yeah sorry darwin ill fix the global later

CppScripts::Script* CppScripts::GetScript(Entity* parent, const std::string& scriptName) {
	Script* script;

	if (m_Scripts.find(scriptName) != m_Scripts.end()) {
		script = m_Scripts[scriptName];

		return script;
	}

	script = invalidToReturn;

	//VE / AG:
	if (scriptName == "scripts\\ai\\AG\\L_AG_SHIP_PLAYER_DEATH_TRIGGER.lua")
		script = new AgShipPlayerDeathTrigger();
	else if (scriptName == "scripts\\ai\\NP\\L_NPC_NP_SPACEMAN_BOB.lua")
		script = new NpcNpSpacemanBob();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_SPACE_STUFF.lua") // Broken, will (sometimes) display all animations at once on initial login
		script = new AgSpaceStuff();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_SHIP_PLAYER_SHOCK_SERVER.lua")
		script = new AgShipPlayerShockServer();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_IMAG_SMASHABLE.lua")
		script = new AgImagSmashable();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_STORY_BOX_INTERACT_SERVER.lua")
		script = new StoryBoxInteractServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_BINOCULARS.lua")
		script = new Binoculars();
	else if (scriptName == "scripts\\ai\\WILD\\L_ALL_CRATE_CHICKEN.lua")
		script = new AllCrateChicken();
	else if (scriptName == "scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_SMASHABLE.lua")
		script = new RockHydrantSmashable(); // Broken?
	else if (scriptName == "scripts\\02_server\\Map\\SS\\L_SS_MODULAR_BUILD_SERVER.lua")
		script = new SsModularBuildServer();
	else if (scriptName == "scripts\\02_server\\Map\\Property\\AG_Small\\L_ZONE_AG_PROPERTY.lua")
		script = new ZoneAgProperty();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_POI_MISSION.lua")
		script = new InvalidScript(); // this is done in Entity.cpp, not needed for our implementation
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_TOUCH_MISSION_UPDATE_SERVER.lua")
		script = new TouchMissionUpdateServer();
	else if (scriptName == "scripts\\ai\\AG\\L_ACT_SHARK_PLAYER_DEATH_TRIGGER.lua")
		script = new ActSharkPlayerDeathTrigger();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_MECH.lua")
		script = new BaseEnemyMech();
	else if (scriptName == "scripts\\zone\\AG\\L_ZONE_AG_SURVIVAL.lua")
		script = new ZoneAgSurvival();
	else if (scriptName == "scripts\\02_server\\Objects\\L_BUFF_STATION_SERVER.lua")
		script = new AgSurvivalBuffStation();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_BUS_DOOR.lua")
		script = new AgBusDoor();
	else if (scriptName == "scripts\\02_server\\Equipment\\L_MAESTROM_EXTRACTICATOR_SERVER.lua")
		script = new MaestromExtracticatorServer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_AG_CAGED_BRICKS_SERVER.lua")
		script = new AgCagedBricksServer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_WISP_SERVER.lua")
		script = new NpcWispServer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_EPSILON_SERVER.lua")
		script = new NpcEpsilonServer();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_TURRET.lua" || scriptName == "scripts\\ai\\AG\\L_AG_TURRET_FOR_SHIP.lua")
		script = new AgTurret();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_AG_LASER_SENSOR_SERVER.lua")
		script = new AgLaserSensorServer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_LASER_SERVER.lua")
		script = new AgMonumentLaserServer();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_FANS.lua")
		script = new AgFans();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_BIRDS.lua")
		script = new AgMonumentBirds();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_REMOVE_RENTAL_GEAR.lua")
		script = new RemoveRentalGear();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_NJ_ASSISTANT_SERVER.lua")
		script = new NpcNjAssistantServer();
	else if (scriptName == "scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER.lua")
		script = new SpecialImaginePowerupSpawner();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_SALUTING_NPCS.lua")
		script = new AgSalutingNpcs();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_JET_EFFECT_SERVER.lua")
		script = new AgJetEffectServer();
	else if (scriptName == "scripts\\02_server\\Enemy\\AG\\L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua")
		script = new BossSpiderQueenEnemyServer();
	else if (scriptName == "scripts\\02_server\\Map\\Property\\AG_Small\\L_ENEMY_SPIDER_SPAWNER.lua")
		script = new EnemySpiderSpawner();
	else if (scriptName == "scripts/02_server/Map/Property/AG_Small/L_ENEMY_SPIDER_SPAWNER.lua")
		script = new EnemySpiderSpawner();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_QB_Elevator.lua")
		script = new AgQbElevator();
	else if (scriptName == "scripts\\ai\\PROPERTY\\AG\\L_AG_PROP_GUARD.lua")
		script = new AgPropGuard();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_AG_BUGSPRAYER.lua")
		script = new AgBugsprayer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_AG_COURSE_STARTER.lua")
		script = new NpcAgCourseStarter();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_GOAL.lua")
		script = new AgMonumentRaceGoal();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_CANCEL.lua")
		script = new AgMonumentRaceCancel();
	else if (scriptName == "scripts\\02_server\\Map\\AG_Spider_Queen\\L_ZONE_AG_SPIDER_QUEEN.lua")
		script = (ZoneAgProperty*)new ZoneAgSpiderQueen();
	else if (scriptName == "scripts\\02_server\\Map\\AG_Spider_Queen\\L_SPIDER_BOSS_TREASURE_CHEST_SERVER.lua")
		script = new SpiderBossTreasureChestServer();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_COWBOY_SERVER.lua")
		script = new NpcCowboyServer();
	else if (scriptName == "scripts\\02_server\\Map\\Property\\AG_Med\\L_ZONE_AG_MED_PROPERTY.lua")
		script = new ZoneAgMedProperty();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_STROMBIE_PROPERTY.lua")
		script = new AgStromlingProperty();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_DARKLING_MECH.lua")
		script = new BaseEnemyMech();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_DARK_SPIDERLING.lua")
		script = new AgDarkSpiderling();
	else if (scriptName == "scripts\\ai\\PROPERTY\\L_PROP_GUARDS.lua")
		script = new AgPropguards();
	else if (scriptName == "scripts\\ai\\PROPERTY\\L_PROPERTY_FX_DAMAGE.lua")
		script = new PropertyFXDamage();
	else if (scriptName == "scripts\\02_server\\Map\\AG\\L_NPC_PIRATE_SERVER.lua")
		script = new NpcPirateServer();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_PICNIC_BLANKET.lua")
		script = new AgPicnicBlanket();
	else if (scriptName == "scripts\\02_server\\Map\\Property\\L_PROPERTY_BANK_INTERACT_SERVER.lua")
		script = new PropertyBankInteract();
	else if (scriptName == "scripts\\02_server\\Enemy\\VE\\L_VE_MECH.lua")
		script = new VeMech();
	else if (scriptName == "scripts\\02_server\\Map\\VE\\L_MISSION_CONSOLE_SERVER.lua")
		script = new VeMissionConsole();
	else if (scriptName == "scripts\\02_server\\Map\\VE\\L_EPSILON_SERVER.lua")
		script = new VeEpsilonServer();
	// Win32 thinks this if chain is too long, let's cut it up and serve it as a three course meal
	//NS:
	if (scriptName == "scripts\\ai\\NS\\L_NS_MODULAR_BUILD.lua")
		script = new NsModularBuild();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_GET_FACTION_MISSION_SERVER.lua")
		script = new NsGetFactionMissionServer();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_QB_IMAGINATION_STATUE.lua")
		script = new NsQbImaginationStatue();
	else if (scriptName == "scripts\\02_server\\Map\\NS\\CONCERT_CHOICEBUILD_MANAGER_SERVER.lua")
		script = new NsConcertChoiceBuildManager();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_CONCERT_CHOICEBUILD.lua")
		script = new NsConcertChoiceBuild();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_CONCERT_QUICKBUILD.lua")
		script = new NsConcertQuickBuild();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_STAGE_PLATFORMS.lua")
		script = new AgStagePlatforms();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_CONCERT_INSTRUMENT_QB.lua")
		script = new NsConcertInstrument();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_JONNY_FLAG_MISSION_SERVER.lua")
		script = new NsJohnnyMissionServer();
	else if (scriptName == "scripts\\02_server\\Objects\\L_STINKY_FISH_TARGET.lua")
		script = new StinkyFishTarget();
	else if (scriptName == "scripts\\zone\\PROPERTY\\NS\\L_ZONE_NS_PROPERTY.lua")
		script = new ZoneNsProperty();
	else if (scriptName == "scripts\\02_server\\Map\\Property\\NS_Med\\L_ZONE_NS_MED_PROPERTY.lua")
		script = new ZoneNsMedProperty();
	else if (scriptName == "scripts\\02_server\\Map\\NS\\L_NS_TOKEN_CONSOLE_SERVER.lua")
		script = new NsTokenConsoleServer();
	else if (scriptName == "scripts\\02_server\\Map\\NS\\L_NS_LUP_TELEPORT.lua")
		script = new NsLupTeleport();
	else if (scriptName == "scripts\\02_server\\Map\\NS\\Waves\\L_ZONE_NS_WAVES.lua")
		script = new ZoneNsWaves();
	else if (scriptName == "scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HAMMERLING_ENEMY_SERVER.lua")
		script = new WaveBossHammerling();
	else if (scriptName == "scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_APE_ENEMY_SERVER.lua")
		script = (BaseEnemyApe*) new WaveBossApe();
	else if (scriptName == "scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_DARK_SPIDERLING_ENEMY_SERVER.lua")
		script = new WaveBossSpiderling();
	else if (scriptName == "scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HORESEMEN_ENEMY_SERVER.lua")
		script = new WaveBossHorsemen();
	else if (scriptName == "scripts\\02_server\\Minigame\\General\\L_MINIGAME_TREASURE_CHEST_SERVER.lua")
		script = new MinigameTreasureChestServer();
	else if (scriptName == "scripts\\02_server\\Map\\NS\\L_NS_LEGO_CLUB_DOOR.lua")
		script = new NsLegoClubDoor();
	else if (scriptName == "scripts/ai/NS/L_CL_RING.lua")
		script = new ClRing();
	else if (scriptName == "scripts\\ai\\WILD\\L_WILD_AMBIENTS.lua")
		script = new WildAmbients();
	else if (scriptName == "scripts\\ai\\NS\\NS_PP_01\\L_NS_PP_01_TELEPORT.lua")
		script = new PropertyDeathPlane();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_QB_SPAWNER.lua")
		script = new QbSpawner();
	else if (scriptName == "scripts\\ai\\AG\\L_AG_QB_Wall.lua")
		script = new AgQbWall();

	//GF:
	else if (scriptName == "scripts\\02_server\\Map\\GF\\L_GF_TORCH.lua")
		script = new GfTikiTorch();
	else if (scriptName == "scripts\\ai\\GF\\L_SPECIAL_FIREPIT.lua")
		script = new GfCampfire();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_ORGAN.lua")
		script = new GfOrgan();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_BANANA.lua")
		script = new GfBanana();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_BANANA_CLUSTER.lua")
		script = new GfBananaCluster();
	else if (scriptName == "scripts/ai/GF/L_GF_JAILKEEP_MISSION.lua")
		script = new GfJailkeepMission();
	else if (scriptName == "scripts\\ai\\GF\\L_TRIGGER_AMBUSH.lua")
		script = new TriggerAmbush();
	else if (scriptName == "scripts\\02_server\\Map\\GF\\L_GF_CAPTAINS_CANNON.lua")
		script = new GfCaptainsCannon();
	else if (scriptName == "scripts\\02_server\\Map\\GF\\L_MAST_TELEPORT.lua")
		script = new MastTeleport();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_JAIL_WALLS.lua")
		script = new GfJailWalls();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_QB_ENEMY_STUNNER.lua")
		script = new QbEnemyStunner();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_PET_DIG_BUILD.lua")
		script = new PetDigBuild(); // Technically also used once in AG
	else if (scriptName == "scripts\\02_server\\Map\\GF\\L_SPAWN_LION_SERVER.lua")
		script = new SpawnLionServer();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_APE.lua")
		script = new BaseEnemyApe();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_GF_APE_SMASHING_QB.lua")
		script = new GfApeSmashingQB();
	else if (scriptName == "scripts\\zone\\PROPERTY\\GF\\L_ZONE_GF_PROPERTY.lua")
		script = new ZoneGfProperty();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_ARCHWAY.lua")
		script = new GfArchway();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_MAELSTROM_GEYSER.lua")
		script = new GfMaelstromGeyser();
	else if (scriptName == "scripts\\ai\\GF\\L_PIRATE_REP.lua")
		script = new PirateRep();
	else if (scriptName == "scripts\\ai\\GF\\L_GF_PARROT_CRASH.lua")
		script = new GfParrotCrash();

	// SG
	else if (scriptName == "scripts\\ai\\MINIGAME\\SG_GF\\SERVER\\SG_CANNON.lua")
		script = new SGCannon();
	else if (scriptName == "scripts\\ai\\MINIGAME\\SG_GF\\L_ZONE_SG_SERVER.lua")
		script = new ZoneSGServer();

	//PR:
	else if (scriptName == "scripts\\client\\ai\\PR\\L_PR_WHISTLE.lua")
		script = new PrWhistle();
	else if (scriptName == "scripts\\02_server\\Map\\PR\\L_PR_SEAGULL_FLY.lua")
		script = new PrSeagullFly();
	else if (scriptName == "scripts\\ai\\PETS\\L_HYDRANT_SMASHABLE.lua")
		script = new HydrantSmashable();
	else if (scriptName == "scripts\\02_server\\map\\PR\\L_HYDRANT_BROKEN.lua")
		script = new HydrantBroken();
	else if (scriptName == "scripts\\02_server\\Map\\General\\PET_DIG_SERVER.lua" || scriptName == "scripts\\02_server\\Map\\AM\\L_SKELETON_DRAGON_PET_DIG_SERVER.lua")
		script = new PetDigServer();
	else if (scriptName == "scripts\\client\\ai\\PR\\L_CRAB_SERVER.lua")
		script = new CrabServer();
	else if (scriptName == "scripts\\02_server\\Pets\\L_PET_FROM_DIG_SERVER.lua")
		script = new PetFromDigServer();
	else if (scriptName == "scripts\\02_server\\Pets\\L_PET_FROM_OBJECT_SERVER.lua")
		script = new PetFromObjectServer();
	else if (scriptName == "scripts\\02_server\\Pets\\L_DAMAGING_PET.lua")
		script = new DamagingPets();
	else if (scriptName == "scripts\\02_server\\Map\\PR\\L_SPAWN_GRYPHON_SERVER.lua")
		script = new SpawnGryphonServer();

	//FV Scripts:
	else if (scriptName == "scripts\\02_server\\Map\\FV\\L_ACT_CANDLE.lua")
		script = new FvCandle();
	else if (scriptName == "scripts\\02_server\\Map\\FV\\L_ENEMY_RONIN_SPAWNER.lua")
		script = new EnemyRoninSpawner();
	else if (scriptName == "scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_CAVALRY.lua")
		script = new FvMaelstromCavalry();
	else if (scriptName == "scripts\\ai\\FV\\L_ACT_NINJA_TURRET_1.lua")
		script = new ActNinjaTurret();
	else if (scriptName == "scripts\\02_server\\Map\\FV\\L_FV_HORSEMEN_TRIGGER.lua")
		script = new FvHorsemenTrigger();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_FLYING_CREVICE_DRAGON.lua")
		script = new FvFlyingCreviceDragon();
	else if (scriptName == "scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_DRAGON.lua")
		script = new FvMaelstromDragon();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_DRAGON_SMASHING_GOLEM_QB.lua")
		script = new FvDragonSmashingGolemQb();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_TREASURE_CHEST_DRAGON_SERVER.lua")
		script = new TreasureChestDragonServer();
	else if (scriptName == "scripts\\ai\\GENERAL\\L_INSTANCE_EXIT_TRANSFER_PLAYER_TO_LAST_NON_INSTANCE.lua")
		script = new InstanceExitTransferPlayerToLastNonInstance();
	else if (scriptName == "scripts\\ai\\FV\\L_NPC_FREE_GF_NINJAS.lua")
		script = new FvFreeGfNinjas();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_PANDA_SPAWNER_SERVER.lua")
		script = new FvPandaSpawnerServer();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_PANDA_SERVER.lua")
		script = new FvPandaServer();
	else if (scriptName == "scripts\\zone\\PROPERTY\\FV\\L_ZONE_FV_PROPERTY.lua")
		script = new ZoneFvProperty();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_BRICK_PUZZLE_SERVER.lua")
		script = new FvBrickPuzzleServer();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_CONSOLE_LEFT_QUICKBUILD.lua")
		script = new FvConsoleLeftQuickbuild();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_CONSOLE_RIGHT_QUICKBUILD.lua")
		script = new FvConsoleRightQuickbuild();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_FACILITY_BRICK.lua")
		script = new FvFacilityBrick();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_FACILITY_PIPES.lua")
		script = new FvFacilityPipes();
	else if (scriptName == "scripts\\02_server\\Map\\FV\\L_IMG_BRICK_CONSOLE_QB.lua")
		script = new ImgBrickConsoleQB();
	else if (scriptName == "scripts\\ai\\FV\\L_ACT_PARADOX_PIPE_FIX.lua")
		script = new ActParadoxPipeFix();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_NINJA_GUARDS.lua")
		script = new FvNinjaGuard();
	else if (scriptName == "scripts\\ai\\FV\\L_ACT_PASS_THROUGH_WALL.lua")
		script = new FvPassThroughWall();
	else if (scriptName == "scripts\\ai\\FV\\L_ACT_BOUNCE_OVER_WALL.lua")
		script = new FvBounceOverWall();
	else if (scriptName == "scripts\\02_server\\Map\\FV\\L_NPC_FONG.lua")
		script = new FvFong();
	else if (scriptName == "scripts\\ai\\FV\\L_FV_MAELSTROM_GEYSER.lua") {
		script = new FvMaelstromGeyser();
	}

	//Misc:
	if (scriptName == "scripts\\02_server\\Map\\General\\L_EXPLODING_ASSET.lua")
		script = new ExplodingAsset();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_WISHING_WELL_SERVER.lua")
		script = new WishingWellServer();
	else if (scriptName == "scripts\\ai\\ACT\\L_ACT_PLAYER_DEATH_TRIGGER.lua")
		script = new ActPlayerDeathTrigger();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_GROWING_FLOWER_SERVER.lua")
		script = new GrowingFlower();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_TOKEN_CONSOLE_SERVER.lua")
		script = new TokenConsoleServer();
	else if (scriptName == "scripts\\ai\\ACT\\FootRace\\L_ACT_BASE_FOOT_RACE.lua")
		script = new BaseFootRaceManager();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_PROP_PLATFORM.lua")
		script = new PropertyPlatform();
	else if (scriptName == "scripts\\02_server\\Map\\VE\\L_VE_BRICKSAMPLE_SERVER.lua")
		return new VeBricksampleServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_MAIL_BOX_SERVER.lua")
		script = new MailBoxServer();
	else if (scriptName == "scripts\\ai\\ACT\\L_ACT_MINE.lua")
		script = new ActMine();

	//Racing:
	else if (scriptName == "scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_CRATE_SERVER.lua")
		script = new RaceImagineCrateServer();
	else if (scriptName == "scripts\\ai\\ACT\\L_ACT_VEHICLE_DEATH_TRIGGER.lua")
		script = new ActVehicleDeathTrigger();
	else if (scriptName == "scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_POWERUP.lua")
		script = new RaceImaginePowerup();
	else if (scriptName == "scripts\\02_server\\Map\\FV\\Racing\\RACE_MAELSTROM_GEISER.lua")
		script = new RaceMaelstromGeiser();
	else if (scriptName == "scripts\\ai\\RACING\\OBJECTS\\FV_RACE_SMASH_EGG_IMAGINE_SERVER.lua")
		script = new FvRaceSmashEggImagineServer();
	else if (scriptName == "scripts\\ai\\RACING\\OBJECTS\\RACE_SMASH_SERVER.lua")
		script = new RaceSmashServer();

	//NT:
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_SENTINELWALKWAY_SERVER.lua")
		script = new NtSentinelWalkwayServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_PARADOXTELE_SERVER.lua")
		script = new NtParadoxTeleServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_DARKITECT_REVEAL_SERVER.lua")
		script = new NtDarkitectRevealServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_BANK_INTERACT_SERVER.lua")
		script = new BankInteractServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_VENTURESPEEDPAD_SERVER.lua")
		script = new NtVentureSpeedPadServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_VENTURE_CANNON_SERVER.lua")
		script = new NtVentureCannonServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_SERVER.lua")
		script = new NtCombatChallengeServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_DUMMY.lua")
		script = new NtCombatChallengeDummy();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\\\L_NT_COMBAT_EXPLODING_TARGET.lua")
		script = new NtCombatChallengeExplodingDummy();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_BASE_INTERACT_DROP_LOOT_SERVER.lua")
		script = new BaseInteractDropLootServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_ASSEMBLYTUBE_SERVER.lua")
		script = new NtAssemblyTubeServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_PARADOX_PANEL_SERVER.lua")
		script = new NtParadoxPanelServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_IMAG_BEAM_BUFFER.lua")
		script = new NtImagBeamBuffer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_BEAM_IMAGINATION_COLLECTORS.lua")
		script = new NtBeamImaginationCollectors();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_DIRT_CLOUD_SERVER.lua")
		script = new NtDirtCloudServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_CONSOLE_TELEPORT_SERVER.lua")
		script = new NtConsoleTeleportServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_SPAWN_STEGO_SERVER.lua")
		script = new SpawnStegoServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_SPAWN_SABERCAT_SERVER.lua")
		script = new SpawnSaberCatServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_SPAWN_SHRAKE_SERVER.lua")
		script = new SpawnShrakeServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_DUKE_SERVER.lua")
		script = new NtDukeServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_HAEL_SERVER.lua")
		script = new NtHaelServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_OVERBUILD_SERVER.lua")
		script = new NtOverbuildServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_VANDA_SERVER.lua")
		script = new NtVandaServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_FORCE_VOLUME_SERVER.lua")
		script = new ForceVolumeServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_XRAY_SERVER.lua")
		script = new NtXRayServer();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_SLEEPING_GUARD.lua")
		script = new NtSleepingGuard();
	else if (scriptName == "scripts\\02_server\\Map\\NT\\L_NT_IMAGIMETER_VISIBILITY_SERVER.lua") {
		script = new NTImagimeterVisibility();
	}

	//AM:
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_AM_CONSOLE_TELEPORT_SERVER.lua")
		script = new AmConsoleTeleportServer();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_FIN.lua")
		script = new RandomSpawnerFin();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_PIT.lua")
		script = new RandomSpawnerPit();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_STR.lua")
		script = new RandomSpawnerStr();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_ZIP.lua")
		script = new RandomSpawnerZip();
	else if (scriptName == "scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_MECH.lua")
		script = new AmDarklingMech();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_BRIDGE.lua")
		script = new AmBridge();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_DRAW_BRIDGE.lua")
		script = new AmDrawBridge();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR.lua")
		script = new AmShieldGenerator();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR_QUICKBUILD.lua")
		script = new AmShieldGeneratorQuickbuild();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_DROPSHIP_COMPUTER.lua")
		script = new AmDropshipComputer();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SCROLL_READER_SERVER.lua")
		script = new AmScrollReaderServer();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_TEMPLE_SKILL_VOLUME.lua")
		script = new AmTemplateSkillVolume();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF.lua")
		script = new EnemyNjBuff();
	else if (scriptName == "scripts\\02_server\\Enemy\\AM\\L_AM_SKELETON_ENGINEER.lua")
		script = new AmSkeletonEngineer();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL.lua")
		script = new AmSkullkinDrill();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL_STAND.lua")
		script = new AmSkullkinDrillStand();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_SKULLKIN_TOWER.lua")
		script = new AmSkullkinTower();
	else if (scriptName == "scripts\\02_server\\Enemy\\AM\\L_AM_NAMED_DARKLING_DRAGON.lua")
		script = new AmDarklingDragon();
	else if (scriptName == "scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_DRAGON.lua")
		script = new AmDarklingDragon();
	else if (scriptName == "scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_APE.lua")
		script = new BaseEnemyApe();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_BLUE_X.lua")
		script = new AmBlueX();
	else if (scriptName == "scripts\\02_server\\Map\\AM\\L_TEAPOT_SERVER.lua")
		script = new AmTeapotServer();

	// Ninjago
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_GARMADON_CELEBRATION_SERVER.lua")
		script = new NjGarmadonCelebration();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_WU_NPC.lua")
		script = new NjWuNPC();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_SCROLL_CHEST_SERVER.lua")
		script = new NjScrollChestServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_COLE_NPC.lua")
		script = new NjColeNPC();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_JAY_MISSION_ITEMS.lua")
		script = (NjNPCMissionSpinjitzuServer*) new NjJayMissionItems();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_NPC_MISSION_SPINJITZU_SERVER.lua")
		script = new NjNPCMissionSpinjitzuServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_ENEMY_SKELETON_SPAWNER.lua")
		script = new EnemySkeletonSpawner();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_NJ_RAIL_SWITCH.lua")
		script = new NjRailSwitch();
	else if (scriptName == "scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_ACTIVATORS_SERVER.lua")
		script = new NjRailActivatorsServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_POST_SERVER.lua")
		script = new NjRailPostServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\Ninjago\\L_ICE_RAIL_ACTIVATOR_SERVER.lua")
		script = new NjIceRailActivator();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_FALLING_TILE.lua")
		script = new FallingTile();
	else if (scriptName == "scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF_STUN_IMMUNITY.lua")
		script = new EnemyNjBuff();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_IMAGINATION_SHRINE_SERVER.lua")
		script = new ImaginationShrineServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_LIEUTENANT.lua")
		script = new Lieutenant();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_RAIN_OF_ARROWS.lua")
		script = new RainOfArrows();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_CAVE_PRISON_CAGE.lua")
		script = new CavePrisonCage();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\boss_instance\\L_MONASTERY_BOSS_INSTANCE_SERVER.lua")
		script = new NjMonastryBossInstance();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_CATAPULT_BOUNCER_SERVER.lua")
		script = new CatapultBouncerServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_CATAPULT_BASE_SERVER.lua")
		script = new CatapultBaseServer();
	else if (scriptName == "scripts\\02_server\\Map\\General\\Ninjago\\L_NJHUB_LAVA_PLAYER_DEATH_TRIGGER.lua")
		script = new NjhubLavaPlayerDeathTrigger();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_MON_CORE_NOOK_DOORS.lua")
		script = new MonCoreNookDoors();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua")
		script = new MonCoreSmashableDoors();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_FLAME_JET_SERVER.lua")
		script = new FlameJetServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_BURNING_TILE.lua")
		script = new BurningTile();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_SPAWN_EARTH_PET_SERVER.lua")
		script = new NjEarthDragonPetServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_EARTH_PET_SERVER.lua")
		script = new NjEarthPetServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_DRAGON_EMBLEM_CHEST_SERVER.lua")
		script = new NjDragonEmblemChestServer();
	else if (scriptName == "scripts\\02_server\\Map\\njhub\\L_NYA_MISSION_ITEMS.lua")
		script = new NjNyaMissionitems();

	//DLU:
	else if (scriptName == "scripts\\02_server\\DLU\\DLUVanityNPC.lua")
		script = new DLUVanityNPC();

	// Survival minigame
	else if (scriptName == "scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_STROMBIE.lua")
		script = new AgSurvivalStromling();
	else if (scriptName == "scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARKLING_MECH.lua")
		script = new AgSurvivalMech();
	else if (scriptName == "scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARK_SPIDERLING.lua")
		script = new AgSurvivalSpiderling();

	// Scripted equipment
	else if (scriptName == "scripts\\EquipmentScripts\\Sunflower.lua")
		script = new Sunflower();
	else if (scriptName == "scripts/EquipmentScripts/AnvilOfArmor.lua")
		script = new AnvilOfArmor();
	else if (scriptName == "scripts/EquipmentScripts/FountainOfImagination.lua")
		script = new FountainOfImagination();
	else if (scriptName == "scripts/EquipmentScripts/CauldronOfLife.lua")
		script = new CauldronOfLife();
	else if (scriptName == "scripts\\02_server\\Equipment\\L_BOOTYDIG_SERVER.lua")
		script = new BootyDigServer();
	else if (scriptName == "scripts\\EquipmentScripts\\PersonalFortress.lua")
		script = new PersonalFortress();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_PROPERTY_DEVICE.lua")
		script = new PropertyDevice();
	else if (scriptName == "scripts\\02_server\\Map\\General\\L_IMAG_BACKPACK_HEALS_SERVER.lua")
		script = new ImaginationBackpackHealServer();
	else if (scriptName == "scripts\\ai\\GENERAL\\L_LEGO_DIE_ROLL.lua")
		script = new LegoDieRoll();
	else if (scriptName == "scripts\\EquipmentScripts\\BuccaneerValiantShip.lua")
		script = new BuccaneerValiantShip();
	else if (scriptName == "scripts\\EquipmentScripts\\FireFirstSkillonStartup.lua")
		script = new FireFirstSkillonStartup();

	// FB
	else if (scriptName == "scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_BROKEN.lua")
		script = new RockHydrantBroken();
	else if (scriptName == "scripts\\ai\\NS\\L_NS_WH_FANS.lua")
		script = new WhFans();

	// WBL
	else if (scriptName == "scripts\\zone\\LUPs\\WBL_generic_zone.lua")
		script = new WblGenericZone();

	// handle invalid script reporting if the path is greater than zero and it's not an ignored script
	// information not really needed for sys admins but is for developers
	else if (script == invalidToReturn) {
		if ((scriptName.length() > 0) && !((scriptName == "scripts\\02_server\\Enemy\\General\\L_SUSPEND_LUA_AI.lua") ||
			(scriptName == "scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_SPIDERLING.lua") ||
			(scriptName == "scripts\\empty.lua")
			)) Game::logger->LogDebug("CppScripts", "LOT %i attempted to load CppScript for '%s', but returned InvalidScript.", parent->GetLOT(), scriptName.c_str());
	}

	m_Scripts[scriptName] = script;
	return script;
}

std::vector<CppScripts::Script*> CppScripts::GetEntityScripts(Entity* entity) {
	std::vector<CppScripts::Script*> scripts;
	std::vector<ScriptComponent*> comps = entity->GetScriptComponents();
	for (ScriptComponent* scriptComp : comps) {
		if (scriptComp != nullptr) {
			scripts.push_back(scriptComp->GetScript());
		}
	}
	return scripts;
}

CppScripts::Script::Script() {

}

CppScripts::Script::~Script() {

}
