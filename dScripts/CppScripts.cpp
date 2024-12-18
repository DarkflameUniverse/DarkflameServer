//I can feel my soul being torn apart with every script added to this monstrosity.
// skate fast eat trash
// do you think god stays in heaven because he too lives in fear of what he's created?

#include "CppScripts.h"
#include "GameMessages.h"
#include "Entity.h"
#include "ScriptComponent.h"
#include "Game.h"
#include "Logger.h"
#include "InvalidScript.h"

//VE / AG scripts:
#include "AgShipPlayerDeathTrigger.h"
#include "AgShipPlayerShockServer.h"
#include "AgSpaceStuff.h"
#include "AgShipShake.h"
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
#include "FvRaceDragon.h"
#include "FvRacePillarABCServer.h"
#include "FvRacePillarDServer.h"
#include "RaceFireballs.h"
#include "RaceShipLapColumnsServer.h"

// FB Scripts
#include "AgJetEffectServer.h"
#include "AgSalutingNpcs.h"
#include "BossSpiderQueenEnemyServer.h"
#include "RockHydrantSmashable.h"

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
#include "FvRacingColumns.h"

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
#include "FrictionVolumeServer.h"
#include "NTPipeVisibilityServer.h"
#include "NTNaomiDirtServer.h"
#include "MinigameBlueMark.h"
#include "NtNaomiBreadcrumbServer.h"

// DLU Scripts
#include "DLUVanityTeleportingObject.h"

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
#include "WanderingVendor.h"

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
#include "GemPack.h"
#include "ShardArmor.h"
#include "TeslaPack.h"
#include "StunImmunity.h"

// Survival scripts
#include "AgSurvivalStromling.h"
#include "AgSurvivalMech.h"
#include "AgSurvivalSpiderling.h"

// Frostburgh Scripts
#include "RockHydrantBroken.h"
#include "WhFans.h"

// WBL scripts
#include "WblGenericZone.h"

// Alpha Scripts
#include "TriggerGas.h"
#include "ActNinjaSensei.h"

// pickups
#include "SpecialCoinSpawner.h"
#include "SpecialPowerupSpawner.h"
#include "SpecialSpeedBuffSpawner.h"

// Wild Scripts
#include "WildAndScared.h"
#include "WildGfGlowbug.h"
#include "WildAmbientCrab.h"
#include "WildPants.h"
#include "WildNinjaStudent.h"
#include "WildNinjaSensei.h"
#include "WildNinjaBricks.h"
#include "VisToggleNotifierServer.h"
#include "LupGenericInteract.h"
#include "WblRobotCitizen.h"
#include "EnemyClearThreat.h"
#include "AgSpiderBossMessage.h"

#include <map>
#include <string>
#include <functional>

namespace {
	// This is in the translation unit instead of the header to prevent weird linker errors
	InvalidScript InvalidToReturn;
	std::map<std::string, CppScripts::Script*> g_Scripts;
	std::map<std::string, std::function<std::unique_ptr<CppScripts::Script> ()>> scriptLoader = {

		//VE / AG
		{ "scripts\\ai\\AG\\L_AG_SHIP_PLAYER_DEATH_TRIGGER.lua", []() { return std::make_unique<AgShipPlayerDeathTrigger>(); } },
		{"scripts\\ai\\NP\\L_NPC_NP_SPACEMAN_BOB.lua", []() { return std::make_unique<NpcNpSpacemanBob>(); } },
		{"scripts\\ai\\AG\\L_AG_SPACE_STUFF.lua", []() { return std::make_unique<AgSpaceStuff>();} },
		{"scripts\\ai\\AG\\L_AG_SHIP_SHAKE.lua", []() { return std::make_unique<AgShipShake>();}},
		{"scripts\\ai\\AG\\L_AG_SHIP_PLAYER_SHOCK_SERVER.lua", []() { return std::make_unique<AgShipPlayerShockServer>();} },
		{"scripts\\ai\\AG\\L_AG_IMAG_SMASHABLE.lua", []() { return std::make_unique<AgImagSmashable>();} },
		{"scripts\\02_server\\Map\\General\\L_STORY_BOX_INTERACT_SERVER.lua", []() { return std::make_unique<StoryBoxInteractServer>();} },
		{"scripts\\02_server\\Map\\General\\L_BINOCULARS.lua", []() { return std::make_unique<Binoculars>();} },
		{"scripts\\ai\\WILD\\L_ALL_CRATE_CHICKEN.lua", []() { return std::make_unique<AllCrateChicken>();} },
		// Broken? (below)
		{"scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_SMASHABLE.lua", []() { return std::make_unique<RockHydrantSmashable>();} },
		{"scripts\\02_server\\Map\\SS\\L_SS_MODULAR_BUILD_SERVER.lua", []() { return std::make_unique<SsModularBuildServer>();} },
		{"scripts\\02_server\\Map\\Property\\AG_Small\\L_ZONE_AG_PROPERTY.lua", []() { return std::make_unique<ZoneAgProperty>();} },
		// this is done in Entity.cpp, not needed for our implementation (below)
		{"scripts\\02_server\\Map\\General\\L_POI_MISSION.lua", []() { return std::make_unique<InvalidScript>();} },
		{"scripts\\02_server\\Map\\General\\L_TOUCH_MISSION_UPDATE_SERVER.lua", []() { return std::make_unique<TouchMissionUpdateServer>();} },
		{"scripts\\ai\\AG\\L_ACT_SHARK_PLAYER_DEATH_TRIGGER.lua", []() { return std::make_unique<ActSharkPlayerDeathTrigger>();} },
		{"scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_MECH.lua", []() { return std::make_unique<BaseEnemyMech>();} },
		{"scripts\\zone\\AG\\L_ZONE_AG_SURVIVAL.lua", []() { return std::make_unique<ZoneAgSurvival>();} },
		{"scripts\\02_server\\Objects\\L_BUFF_STATION_SERVER.lua", []() { return std::make_unique<AgSurvivalBuffStation>();} },
		{"scripts\\ai\\AG\\L_AG_BUS_DOOR.lua", []() { return std::make_unique<AgBusDoor>();} },
		{"scripts\\02_server\\Equipment\\L_MAESTROM_EXTRACTICATOR_SERVER.lua", []() { return std::make_unique<MaestromExtracticatorServer>();} },
		{"scripts\\02_server\\Map\\AG\\L_AG_CAGED_BRICKS_SERVER.lua", []() { return std::make_unique<AgCagedBricksServer>();} },
		{"scripts\\02_server\\Map\\AG\\L_NPC_WISP_SERVER.lua", []() { return std::make_unique<NpcWispServer>();} },
		{"scripts\\02_server\\Map\\AG\\L_NPC_EPSILON_SERVER.lua", []() { return std::make_unique<NpcEpsilonServer>();} },
		{"scripts\\ai\\AG\\L_AG_TURRET.lua", []() {return std::make_unique<AgTurret>();}},
		{"scripts\\ai\\AG\\L_AG_TURRET_FOR_SHIP.lua", []() { return std::make_unique<AgTurret>();}},
		{"scripts\\02_server\\Map\\AG\\L_AG_LASER_SENSOR_SERVER.lua", []() {return std::make_unique<AgLaserSensorServer>();}},
		{"scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_LASER_SERVER.lua", []() {return std::make_unique<AgMonumentLaserServer>();}},
		{"scripts\\ai\\AG\\L_AG_FANS.lua", []() {return std::make_unique<AgFans>();}},
		{"scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_BIRDS.lua", []() {return std::make_unique<AgMonumentBirds>();}},
		{"scripts\\02_server\\Map\\AG\\L_REMOVE_RENTAL_GEAR.lua", []() {return std::make_unique<RemoveRentalGear>();}},
		{"scripts\\02_server\\Map\\AG\\L_NPC_NJ_ASSISTANT_SERVER.lua", []() {return std::make_unique<NpcNjAssistantServer>();}},
		{"scripts\\ai\\AG\\L_AG_SALUTING_NPCS.lua", []() {return std::make_unique<AgSalutingNpcs>();}},
		{"scripts\\ai\\AG\\L_AG_JET_EFFECT_SERVER.lua", []() {return std::make_unique<AgJetEffectServer>();}},
		{"scripts\\02_server\\Enemy\\AG\\L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua", []() {return std::make_unique<BossSpiderQueenEnemyServer>();}},
		{"scripts\\02_server\\Map\\Property\\AG_Small\\L_ENEMY_SPIDER_SPAWNER.lua", []() {return std::make_unique<EnemySpiderSpawner>();}},
		{"scripts/02_server/Map/Property/AG_Small/L_ENEMY_SPIDER_SPAWNER.lua", []() {return std::make_unique<EnemySpiderSpawner>();}},
		{"scripts\\ai\\AG\\L_AG_QB_Elevator.lua", []() {return std::make_unique<AgQbElevator>();}},
		{"scripts\\ai\\PROPERTY\\AG\\L_AG_PROP_GUARD.lua", []() {return std::make_unique<AgPropGuard>();}},
		{"scripts\\02_server\\Map\\AG\\L_AG_BUGSPRAYER.lua", []() {return std::make_unique<AgBugsprayer>();}},
		{"scripts\\02_server\\Map\\AG\\L_NPC_AG_COURSE_STARTER.lua", []() {return std::make_unique<NpcAgCourseStarter>();}},
		{"scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_GOAL.lua", []() {return std::make_unique<AgMonumentRaceGoal>();}},
		{"scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_CANCEL.lua", []() {return std::make_unique<AgMonumentRaceCancel>();}},
		{"scripts\\02_server\\Map\\AG_Spider_Queen\\L_ZONE_AG_SPIDER_QUEEN.lua", []() {return std::make_unique<ZoneAgSpiderQueen>();}},
		{"scripts\\02_server\\Map\\AG_Spider_Queen\\L_SPIDER_BOSS_TREASURE_CHEST_SERVER.lua", []() {return std::make_unique<SpiderBossTreasureChestServer>();}},
		{"scripts\\02_server\\Map\\AG\\L_NPC_COWBOY_SERVER.lua", []() {return std::make_unique<NpcCowboyServer>();}},
		{"scripts\\02_server\\Map\\Property\\AG_Med\\L_ZONE_AG_MED_PROPERTY.lua", []() {return std::make_unique<ZoneAgMedProperty>();}},
		{"scripts\\ai\\AG\\L_AG_STROMBIE_PROPERTY.lua", []() {return std::make_unique<AgStromlingProperty>();}},
		{"scripts\\ai\\AG\\L_AG_DARKLING_MECH.lua", []() {return std::make_unique<BaseEnemyMech>();}},
		{"scripts\\ai\\AG\\L_AG_DARK_SPIDERLING.lua", []() {return std::make_unique<AgDarkSpiderling>();}},
		{"scripts\\ai\\PROPERTY\\L_PROP_GUARDS.lua", []() {return std::make_unique<AgPropguards>();}},
		{"scripts\\ai\\PROPERTY\\L_PROPERTY_FX_DAMAGE.lua", []() {return std::make_unique<PropertyFXDamage>();}},
		{"scripts\\02_server\\Map\\AG\\L_NPC_PIRATE_SERVER.lua", []() {return std::make_unique<NpcPirateServer>();}},
		{"scripts\\ai\\AG\\L_AG_PICNIC_BLANKET.lua", []() {return std::make_unique<AgPicnicBlanket>();}},
		{"scripts\\02_server\\Map\\Property\\L_PROPERTY_BANK_INTERACT_SERVER.lua", []() {return std::make_unique<PropertyBankInteract>();}},
		{"scripts\\02_server\\Enemy\\VE\\L_VE_MECH.lua", []() {return std::make_unique<VeMech>();}},
		{"scripts\\02_server\\Map\\VE\\L_MISSION_CONSOLE_SERVER.lua", []() {return std::make_unique<VeMissionConsole>();}},
		{"scripts\\02_server\\Map\\VE\\L_EPSILON_SERVER.lua", []() {return std::make_unique<VeEpsilonServer>();}},

		//NS
		{"scripts\\ai\\NS\\L_NS_MODULAR_BUILD.lua", []() {return std::make_unique<NsModularBuild>();}},
		{"scripts\\ai\\NS\\L_NS_GET_FACTION_MISSION_SERVER.lua", []() {return std::make_unique<NsGetFactionMissionServer>();}},
		{"scripts\\ai\\NS\\L_NS_QB_IMAGINATION_STATUE.lua", []() {return std::make_unique<NsQbImaginationStatue>();}},
		{"scripts\\02_server\\Map\\NS\\CONCERT_CHOICEBUILD_MANAGER_SERVER.lua", []() {return std::make_unique<NsConcertChoiceBuildManager>();}},
		{"scripts\\ai\\NS\\L_NS_CONCERT_CHOICEBUILD.lua", []() {return std::make_unique<NsConcertChoiceBuild>();}},
		{"scripts\\ai\\NS\\L_NS_CONCERT_QUICKBUILD.lua", []() {return std::make_unique<NsConcertQuickBuild>();}},
		{"scripts\\ai\\AG\\L_AG_STAGE_PLATFORMS.lua", []() {return std::make_unique<AgStagePlatforms>();}},
		{"scripts\\ai\\NS\\L_NS_CONCERT_INSTRUMENT_QB.lua", []() {return std::make_unique<NsConcertInstrument>();}},
		{"scripts\\ai\\NS\\L_NS_JONNY_FLAG_MISSION_SERVER.lua", []() {return std::make_unique<NsJohnnyMissionServer>();}},
		{"scripts\\02_server\\Objects\\L_STINKY_FISH_TARGET.lua", []() {return std::make_unique<StinkyFishTarget>();}},
		{"scripts\\zone\\PROPERTY\\NS\\L_ZONE_NS_PROPERTY.lua", []() {return std::make_unique<ZoneNsProperty>();}},
		{"scripts\\02_server\\Map\\Property\\NS_Med\\L_ZONE_NS_MED_PROPERTY.lua", []() {return std::make_unique<ZoneNsMedProperty>();}},
		{"scripts\\02_server\\Map\\NS\\L_NS_TOKEN_CONSOLE_SERVER.lua", []() {return std::make_unique<NsTokenConsoleServer>();}},
		{"scripts\\02_server\\Map\\NS\\L_NS_LUP_TELEPORT.lua", []() {return std::make_unique<NsLupTeleport>();}},
		{"scripts\\02_server\\Map\\NS\\Waves\\L_ZONE_NS_WAVES.lua", []() {return std::make_unique<ZoneNsWaves>();}},
		{"scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HAMMERLING_ENEMY_SERVER.lua", []() {return std::make_unique<WaveBossHammerling>();}},
		{"scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_APE_ENEMY_SERVER.lua", []() {return std::make_unique<WaveBossApe>();}},
		{"scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_DARK_SPIDERLING_ENEMY_SERVER.lua", []() {return std::make_unique<WaveBossSpiderling>();}},
		{"scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HORESEMEN_ENEMY_SERVER.lua", []() {return std::make_unique<WaveBossHorsemen>();}},
		{"scripts\\02_server\\Minigame\\General\\L_MINIGAME_TREASURE_CHEST_SERVER.lua", []() {return std::make_unique<MinigameTreasureChestServer>();}},
		{"scripts\\02_server\\Map\\NS\\L_NS_LEGO_CLUB_DOOR.lua", []() {return std::make_unique<NsLegoClubDoor>();}},
		{"scripts/ai/NS/L_CL_RING.lua", []() {return std::make_unique<ClRing>();}},
		{"scripts\\ai\\WILD\\L_WILD_AMBIENTS.lua", []() {return std::make_unique<WildAmbients>();}},
		{"scripts\\ai\\NS\\NS_PP_01\\L_NS_PP_01_TELEPORT.lua", []() {return std::make_unique<PropertyDeathPlane>();}},
		{"scripts\\02_server\\Map\\General\\L_QB_SPAWNER.lua", []() {return std::make_unique<QbSpawner>();}},
		{"scripts\\ai\\AG\\L_AG_QB_Wall.lua", []() {return std::make_unique<AgQbWall>();}},

		//GF
		{"scripts\\02_server\\Map\\GF\\L_GF_TORCH.lua", []() {return std::make_unique<GfTikiTorch>();}},
		{"scripts\\ai\\GF\\L_SPECIAL_FIREPIT.lua", []() {return std::make_unique<GfCampfire>();}},
		{"scripts\\ai\\GF\\L_GF_ORGAN.lua", []() {return std::make_unique<GfOrgan>();}},
		{"scripts\\ai\\GF\\L_GF_BANANA.lua", []() {return std::make_unique<GfBanana>();}},
		{"scripts\\ai\\GF\\L_GF_BANANA_CLUSTER.lua", []() {return std::make_unique<GfBananaCluster>();}},
		{"scripts/ai/GF/L_GF_JAILKEEP_MISSION.lua", []() {return std::make_unique<GfJailkeepMission>();}},
		{"scripts\\ai\\GF\\L_TRIGGER_AMBUSH.lua", []() {return std::make_unique<TriggerAmbush>();}},
		{"scripts\\02_server\\Map\\GF\\L_GF_CAPTAINS_CANNON.lua", []() {return std::make_unique<GfCaptainsCannon>();}},
		{"scripts\\02_server\\Map\\GF\\L_MAST_TELEPORT.lua", []() {return std::make_unique<MastTeleport>();}},
		{"scripts\\ai\\GF\\L_GF_JAIL_WALLS.lua", []() {return std::make_unique<GfJailWalls>();}},
		{"scripts\\02_server\\Map\\General\\L_QB_ENEMY_STUNNER.lua", []() {return std::make_unique<QbEnemyStunner>();}},
		//Technically also used once in AG (below)
		{"scripts\\ai\\GF\\L_GF_PET_DIG_BUILD.lua", []() {return std::make_unique<PetDigBuild>();}},
		{"scripts\\02_server\\Map\\GF\\L_SPAWN_LION_SERVER.lua", []() {return std::make_unique<SpawnLionServer>();}},
		{"scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_APE.lua", []() {return std::make_unique<BaseEnemyApe>();}},
		{"scripts\\02_server\\Enemy\\General\\L_GF_APE_SMASHING_QB.lua", []() {return std::make_unique<GfApeSmashingQB>();}},
		{"scripts\\zone\\PROPERTY\\GF\\L_ZONE_GF_PROPERTY.lua", []() {return std::make_unique<ZoneGfProperty>();}},
		{"scripts\\ai\\GF\\L_GF_ARCHWAY.lua", []() {return std::make_unique<GfArchway>();}},
		{"scripts\\ai\\GF\\L_GF_MAELSTROM_GEYSER.lua", []() {return std::make_unique<GfMaelstromGeyser>();}},
		{"scripts\\ai\\GF\\L_PIRATE_REP.lua", []() {return std::make_unique<PirateRep>();}},
		{"scripts\\ai\\GF\\L_GF_PARROT_CRASH.lua", []() {return std::make_unique<GfParrotCrash>();}},

		//SG
		{"scripts\\ai\\MINIGAME\\SG_GF\\SERVER\\SG_CANNON.lua", []() {return std::make_unique<SGCannon>();}},
		{"scripts\\ai\\MINIGAME\\SG_GF\\L_ZONE_SG_SERVER.lua", []() {return std::make_unique<ZoneSGServer>();}},

		//PR
		{"scripts\\client\\ai\\PR\\L_PR_WHISTLE.lua", []() {return std::make_unique<PrWhistle>();}},
		{"scripts\\02_server\\Map\\PR\\L_PR_SEAGULL_FLY.lua", []() {return std::make_unique<PrSeagullFly>();}},
		{"scripts\\ai\\PETS\\L_HYDRANT_SMASHABLE.lua", []() {return std::make_unique<HydrantSmashable>();}},
		{"scripts\\02_server\\map\\PR\\L_HYDRANT_BROKEN.lua", []() {return std::make_unique<HydrantBroken>();}},
		{"scripts\\02_server\\Map\\General\\PET_DIG_SERVER.lua", []() {return std::make_unique<PetDigServer>();}},
		{"scripts\\02_server\\Map\\AM\\L_SKELETON_DRAGON_PET_DIG_SERVER.lua", []() {return std::make_unique<PetDigServer>();}},
		//{"scripts\\02_server\\Map\\AM\\L_SKELETON_DRAGON_PET_DIG_SERVER.lua", [](){return std::make_unique<PetDigServer>();}},
		{"scripts\\client\\ai\\PR\\L_CRAB_SERVER.lua", []() {return std::make_unique<CrabServer>();}},
		{"scripts\\02_server\\Pets\\L_PET_FROM_DIG_SERVER.lua", []() {return std::make_unique<PetFromDigServer>();}},
		{"scripts\\02_server\\Pets\\L_PET_FROM_OBJECT_SERVER.lua", []() {return std::make_unique<PetFromObjectServer>();}},
		{"scripts\\02_server\\Pets\\L_DAMAGING_PET.lua", []() {return std::make_unique<DamagingPets>();}},
		{"scripts\\02_server\\Map\\PR\\L_SPAWN_GRYPHON_SERVER.lua", []() {return std::make_unique<SpawnGryphonServer>();}},

		//FV
		{"scripts\\02_server\\Map\\FV\\L_ACT_CANDLE.lua", []() {return std::make_unique<FvCandle>();}},
		{"scripts\\02_server\\Map\\FV\\L_ENEMY_RONIN_SPAWNER.lua", []() {return std::make_unique<EnemyRoninSpawner>();}},
		{"scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_CAVALRY.lua", []() {return std::make_unique<FvMaelstromCavalry>();}},
		{"scripts\\ai\\FV\\L_ACT_NINJA_TURRET_1.lua", []() {return std::make_unique<ActNinjaTurret>();}},
		{"scripts\\02_server\\Map\\FV\\L_FV_HORSEMEN_TRIGGER.lua", []() {return std::make_unique<FvHorsemenTrigger>();}},
		{"scripts\\ai\\FV\\L_FV_FLYING_CREVICE_DRAGON.lua", []() {return std::make_unique<FvFlyingCreviceDragon>();}},
		{"scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_DRAGON.lua", []() {return std::make_unique<FvMaelstromDragon>();}},
		{"scripts\\ai\\FV\\L_FV_DRAGON_SMASHING_GOLEM_QB.lua", []() {return std::make_unique<FvDragonSmashingGolemQb>();}},
		{"scripts\\02_server\\Enemy\\General\\L_TREASURE_CHEST_DRAGON_SERVER.lua", []() {return std::make_unique<TreasureChestDragonServer>();}},
		{"scripts\\ai\\GENERAL\\L_INSTANCE_EXIT_TRANSFER_PLAYER_TO_LAST_NON_INSTANCE.lua", []() {return std::make_unique<InstanceExitTransferPlayerToLastNonInstance>();}},
		{"scripts\\ai\\FV\\L_NPC_FREE_GF_NINJAS.lua", []() {return std::make_unique<FvFreeGfNinjas>();}},
		{"scripts\\ai\\FV\\L_FV_PANDA_SPAWNER_SERVER.lua", []() {return std::make_unique<FvPandaSpawnerServer>();}},
		{"scripts\\ai\\FV\\L_FV_PANDA_SERVER.lua", []() {return std::make_unique<FvPandaServer>();}},
		{"scripts\\zone\\PROPERTY\\FV\\L_ZONE_FV_PROPERTY.lua", []() {return std::make_unique<ZoneFvProperty>();}},
		{"scripts\\ai\\FV\\L_FV_BRICK_PUZZLE_SERVER.lua", []() {return std::make_unique<FvBrickPuzzleServer>();}},
		{"scripts\\ai\\FV\\L_FV_CONSOLE_LEFT_QUICKBUILD.lua", []() {return std::make_unique<FvConsoleLeftQuickbuild>();}},
		{"scripts\\ai\\FV\\L_FV_CONSOLE_RIGHT_QUICKBUILD.lua", []() {return std::make_unique<FvConsoleRightQuickbuild>();}},
		{"scripts\\ai\\FV\\L_FV_FACILITY_BRICK.lua", []() {return std::make_unique<FvFacilityBrick>();}},
		{"scripts\\ai\\FV\\L_FV_FACILITY_PIPES.lua", []() {return std::make_unique<FvFacilityPipes>();}},
		{"scripts\\02_server\\Map\\FV\\L_IMG_BRICK_CONSOLE_QB.lua", []() {return std::make_unique<ImgBrickConsoleQB>();}},
		{"scripts\\ai\\FV\\L_ACT_PARADOX_PIPE_FIX.lua", []() {return std::make_unique<ActParadoxPipeFix>();}},
		{"scripts\\ai\\FV\\L_FV_NINJA_GUARDS.lua", []() {return std::make_unique<FvNinjaGuard>();}},
		{"scripts\\ai\\FV\\L_ACT_PASS_THROUGH_WALL.lua", []() {return std::make_unique<FvPassThroughWall>();}},
		{"scripts\\ai\\FV\\L_ACT_BOUNCE_OVER_WALL.lua", []() {return std::make_unique<FvBounceOverWall>();}},
		{"scripts\\02_server\\Map\\FV\\L_NPC_FONG.lua", []() {return std::make_unique<FvFong>();}},
		{"scripts\\ai\\FV\\L_FV_MAELSTROM_GEYSER.lua", []() {return std::make_unique<FvMaelstromGeyser>();}},
		{"scripts\\02_server\\Map\\FV\\Racing\\RACE_SHIP_LAP_COLUMNS_SERVER.lua", []() {return std::make_unique<RaceShipLapColumnsServer>();}},

		//yes we know the lap numbers dont match the file name or anim. Thats what they desgined it as.
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP1_SERVER.lua", []() {return std::make_unique<FvRaceDragon>("lap_01", 2);}},
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP2_SERVER.lua", []() {return std::make_unique<FvRaceDragon>("lap_02", 0);}},
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP3_SERVER.lua", []() {return std::make_unique<FvRaceDragon>("lap_03", 1);}},
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_ABC_SERVER.lua", []() {return std::make_unique<FvRacePillarABCServer>();}},
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_D_SERVER.lua", []() {return std::make_unique<FvRacePillarDServer>();}},
		{"scripts\\02_server\\Map\\FV\\Racing\\RACE_FIREBALLS.lua", []() {return std::make_unique<RaceFireballs>();}},

		//Misc.
		{"scripts\\02_server\\Map\\General\\L_EXPLODING_ASSET.lua", []() {return std::make_unique<ExplodingAsset>();}},
		{"scripts\\02_server\\Map\\General\\L_WISHING_WELL_SERVER.lua", []() {return std::make_unique<WishingWellServer>();}},
		{"scripts\\ai\\ACT\\L_ACT_PLAYER_DEATH_TRIGGER.lua", []() {return std::make_unique<ActPlayerDeathTrigger>();}},
		{"scripts\\02_server\\Map\\General\\L_GROWING_FLOWER_SERVER.lua", []() {return std::make_unique<GrowingFlower>();}},
		{"scripts\\02_server\\Map\\General\\L_TOKEN_CONSOLE_SERVER.lua", []() {return std::make_unique<TokenConsoleServer>();}},
		{"scripts\\ai\\ACT\\FootRace\\L_ACT_BASE_FOOT_RACE.lua", []() {return std::make_unique<BaseFootRaceManager>();}},
		{"scripts\\02_server\\Map\\General\\L_PROP_PLATFORM.lua", []() {return std::make_unique<PropertyPlatform>();}},
		{"scripts\\02_server\\Map\\VE\\L_VE_BRICKSAMPLE_SERVER.lua", []() {return std::make_unique<VeBricksampleServer>();}},
		{"scripts\\02_server\\Map\\General\\L_MAIL_BOX_SERVER.lua", []() {return std::make_unique<MailBoxServer>();}},
		{"scripts\\ai\\ACT\\L_ACT_MINE.lua", []() {return std::make_unique<ActMine>();}},
		{"scripts\\02_server\\Map\\AM\\L_WANDERING_VENDOR.lua", []() {return std::make_unique<WanderingVendor>();}},

		//Racing
		{"scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_CRATE_SERVER.lua", []() {return std::make_unique<RaceImagineCrateServer>();}},
		{"scripts\\ai\\ACT\\L_ACT_VEHICLE_DEATH_TRIGGER.lua", []() {return std::make_unique<ActVehicleDeathTrigger>();}},
		{"scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_POWERUP.lua", []() {return std::make_unique<RaceImaginePowerup>();}},
		{"scripts\\02_server\\Map\\FV\\Racing\\RACE_MAELSTROM_GEISER.lua", []() {return std::make_unique<RaceMaelstromGeiser>();}},
		{"scripts\\ai\\RACING\\OBJECTS\\FV_RACE_SMASH_EGG_IMAGINE_SERVER.lua", []() {return std::make_unique<FvRaceSmashEggImagineServer>();}},
		{"scripts\\02_server\\Map\\FV\\Racing\\FV_RACING_COLUMNS.lua", []() {return std::make_unique<FvRacingColumns>();}},
		{"scripts\\ai\\RACING\\OBJECTS\\RACE_SMASH_SERVER.lua", []() {return std::make_unique<RaceSmashServer>();}},

		//NT
		{"scripts\\02_server\\Map\\NT\\L_NT_SENTINELWALKWAY_SERVER.lua", []() {return std::make_unique<NtSentinelWalkwayServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_PARADOXTELE_SERVER.lua", []() {return std::make_unique<NtParadoxTeleServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_DARKITECT_REVEAL_SERVER.lua", []() {return std::make_unique<NtDarkitectRevealServer>();}},
		{"scripts\\02_server\\Map\\General\\L_BANK_INTERACT_SERVER.lua", []() {return std::make_unique<BankInteractServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_VENTURESPEEDPAD_SERVER.lua", []() {return std::make_unique<NtVentureSpeedPadServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_VENTURE_CANNON_SERVER.lua", []() {return std::make_unique<NtVentureCannonServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_SERVER.lua", []() {return std::make_unique<NtCombatChallengeServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_DUMMY.lua", []() {return std::make_unique<NtCombatChallengeDummy>();}},
		{"scripts\\02_server\\Map\\NT\\\\L_NT_COMBAT_EXPLODING_TARGET.lua", []() {return std::make_unique<NtCombatChallengeExplodingDummy>();}},
		{"scripts\\02_server\\Map\\General\\L_BASE_INTERACT_DROP_LOOT_SERVER.lua", []() {return std::make_unique<BaseInteractDropLootServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_ASSEMBLYTUBE_SERVER.lua", []() {return std::make_unique<NtAssemblyTubeServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_PARADOX_PANEL_SERVER.lua", []() {return std::make_unique<NtParadoxPanelServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_IMAG_BEAM_BUFFER.lua", []() {return std::make_unique<NtImagBeamBuffer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_BEAM_IMAGINATION_COLLECTORS.lua", []() {return std::make_unique<NtBeamImaginationCollectors>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_DIRT_CLOUD_SERVER.lua", []() {return std::make_unique<NtDirtCloudServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_CONSOLE_TELEPORT_SERVER.lua", []() {return std::make_unique<NtConsoleTeleportServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_SPAWN_STEGO_SERVER.lua", []() {return std::make_unique<SpawnStegoServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_SPAWN_SABERCAT_SERVER.lua", []() {return std::make_unique<SpawnSaberCatServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_SPAWN_SHRAKE_SERVER.lua", []() {return std::make_unique<SpawnShrakeServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_DUKE_SERVER.lua", []() {return std::make_unique<NtDukeServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_HAEL_SERVER.lua", []() {return std::make_unique<NtHaelServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_FACTION_SPY_SERVER.lua", []() {return std::make_unique<NtFactionSpyServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_OVERBUILD_SERVER.lua", []() {return std::make_unique<NtOverbuildServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_VANDA_SERVER.lua", []() {return std::make_unique<NtVandaServer>();}},
		{"scripts\\02_server\\Map\\General\\L_FORCE_VOLUME_SERVER.lua", []() {return std::make_unique<ForceVolumeServer>();}},
		{"scripts\\02_server\\Map\\General\\L_FRICTION_VOLUME_SERVER.lua", []() {return std::make_unique<FrictionVolumeServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_XRAY_SERVER.lua", []() {return std::make_unique<NtXRayServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_SLEEPING_GUARD.lua", []() {return std::make_unique<NtSleepingGuard>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_IMAGIMETER_VISIBILITY_SERVER.lua", []() {return std::make_unique<NTImagimeterVisibility>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_PIPE_VISIBILITY_SERVER.lua", []() {return std::make_unique<NTPipeVisibilityServer>();}},
		{"scripts\\ai\\MINIGAME\\Objects\\MINIGAME_BLUE_MARK.lua", []() {return std::make_unique<MinigameBlueMark>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_NAOMI_BREADCRUMB_SERVER.lua", []() {return std::make_unique<NtNaomiBreadcrumbServer>();}},
		{"scripts\\02_server\\Map\\NT\\L_NT_NAOMI_DIRT_SERVER.lua", []() {return std::make_unique<NTNaomiDirtServer>();}},

		//AM Crux
		{"scripts\\02_server\\Map\\AM\\L_AM_CONSOLE_TELEPORT_SERVER.lua", []() {return std::make_unique<AmConsoleTeleportServer>();}},
		{"scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_FIN.lua", []() {return std::make_unique<RandomSpawnerFin>();}},
		{"scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_PIT.lua", []() {return std::make_unique<RandomSpawnerPit>();}},
		{"scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_STR.lua", []() {return std::make_unique<RandomSpawnerStr>();}},
		{"scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_ZIP.lua", []() {return std::make_unique<RandomSpawnerZip>();}},
		{"scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_MECH.lua", []() {return std::make_unique<AmDarklingMech>();}},
		{"scripts\\02_server\\Map\\AM\\L_BRIDGE.lua", []() {return std::make_unique<AmBridge>();}},
		{"scripts\\02_server\\Map\\AM\\L_DRAW_BRIDGE.lua", []() {return std::make_unique<AmDrawBridge>();}},
		{"scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR.lua", []() {return std::make_unique<AmShieldGenerator>();}},
		{"scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR_QUICKBUILD.lua", []() {return std::make_unique<AmShieldGeneratorQuickbuild>();}},
		{"scripts\\02_server\\Map\\AM\\L_DROPSHIP_COMPUTER.lua", []() {return std::make_unique<AmDropshipComputer>();}},
		{"scripts\\02_server\\Map\\AM\\L_SCROLL_READER_SERVER.lua", []() {return std::make_unique<AmScrollReaderServer>();}},
		{"scripts\\02_server\\Map\\AM\\L_TEMPLE_SKILL_VOLUME.lua", []() {return std::make_unique<AmTemplateSkillVolume>();}},
		{"scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF.lua", []() {return std::make_unique<EnemyNjBuff>();}},
		{"scripts\\02_server\\Enemy\\AM\\L_AM_SKELETON_ENGINEER.lua", []() {return std::make_unique<AmSkeletonEngineer>();}},
		{"scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL.lua", []() {return std::make_unique<AmSkullkinDrill>();}},
		{"scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL_STAND.lua", []() {return std::make_unique<AmSkullkinDrillStand>();}},
		{"scripts\\02_server\\Map\\AM\\L_SKULLKIN_TOWER.lua", []() {return std::make_unique<AmSkullkinTower>();}},
		{"scripts\\02_server\\Enemy\\AM\\L_AM_NAMED_DARKLING_DRAGON.lua", []() {return std::make_unique<AmDarklingDragon>();}},
		{"scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_DRAGON.lua", []() {return std::make_unique<AmDarklingDragon>();}},
		{"scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_APE.lua", []() {return std::make_unique<BaseEnemyApe>();}},
		{"scripts\\02_server\\Map\\AM\\L_BLUE_X.lua", []() {return std::make_unique<AmBlueX>();}},
		{"scripts\\02_server\\Map\\AM\\L_TEAPOT_SERVER.lua", []() {return std::make_unique<AmTeapotServer>();}},

		//Ninjago
		{"scripts\\02_server\\Map\\njhub\\L_GARMADON_CELEBRATION_SERVER.lua", []() {return std::make_unique<NjGarmadonCelebration>();}},
		{"scripts\\02_server\\Map\\njhub\\L_WU_NPC.lua", []() {return std::make_unique<NjWuNPC>();}},
		{"scripts\\02_server\\Map\\njhub\\L_SCROLL_CHEST_SERVER.lua", []() {return std::make_unique<NjScrollChestServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_COLE_NPC.lua", []() {return std::make_unique<NjColeNPC>();}},
		{"scripts\\02_server\\Map\\njhub\\L_JAY_MISSION_ITEMS.lua", []() {return std::make_unique<NjJayMissionItems>();}},
		{"scripts\\02_server\\Map\\njhub\\L_NPC_MISSION_SPINJITZU_SERVER.lua", []() {return std::make_unique<NjNPCMissionSpinjitzuServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_ENEMY_SKELETON_SPAWNER.lua", []() {return std::make_unique<EnemySkeletonSpawner>();}},
		{"scripts\\02_server\\Map\\General\\L_NJ_RAIL_SWITCH.lua", []() {return std::make_unique<NjRailSwitch>();}},
		{"scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_ACTIVATORS_SERVER.lua", []() {return std::make_unique<NjRailActivatorsServer>();}},
		{"scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_POST_SERVER.lua", []() {return std::make_unique<NjRailPostServer>();}},
		{"scripts\\02_server\\Map\\General\\Ninjago\\L_ICE_RAIL_ACTIVATOR_SERVER.lua", []() {return std::make_unique<NjIceRailActivator>();}},
		{"scripts\\02_server\\Map\\njhub\\L_FALLING_TILE.lua", []() {return std::make_unique<FallingTile>();}},
		{"scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF_STUN_IMMUNITY.lua", []() {return std::make_unique<EnemyNjBuff>();}},
		{"scripts\\02_server\\Map\\njhub\\L_IMAGINATION_SHRINE_SERVER.lua", []() {return std::make_unique<ImaginationShrineServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_LIEUTENANT.lua", []() {return std::make_unique<Lieutenant>();}},
		{"scripts\\02_server\\Map\\njhub\\L_RAIN_OF_ARROWS.lua", []() {return std::make_unique<RainOfArrows>();}},
		{"scripts\\02_server\\Map\\njhub\\L_CAVE_PRISON_CAGE.lua", []() {return std::make_unique<CavePrisonCage>();}},
		{"scripts\\02_server\\Map\\njhub\\boss_instance\\L_MONASTERY_BOSS_INSTANCE_SERVER.lua", []() {return std::make_unique<NjMonastryBossInstance>();}},
		{"scripts\\02_server\\Map\\njhub\\L_CATAPULT_BOUNCER_SERVER.lua", []() {return std::make_unique<CatapultBouncerServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_CATAPULT_BASE_SERVER.lua", []() {return std::make_unique<CatapultBaseServer>();}},
		{"scripts\\02_server\\Map\\General\\Ninjago\\L_NJHUB_LAVA_PLAYER_DEATH_TRIGGER.lua", []() {return std::make_unique<NjhubLavaPlayerDeathTrigger>();}},
		{"scripts\\02_server\\Map\\njhub\\L_MON_CORE_NOOK_DOORS.lua", []() {return std::make_unique<MonCoreNookDoors>();}},
		{"scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua", []() {return std::make_unique<MonCoreSmashableDoors>();}},
		{"scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua", []() {return std::make_unique<MonCoreSmashableDoors>();}},
		{"scripts\\02_server\\Map\\njhub\\L_FLAME_JET_SERVER.lua", []() {return std::make_unique<FlameJetServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_BURNING_TILE.lua", []() {return std::make_unique<BurningTile>();}},
		{"scripts\\02_server\\Map\\njhub\\L_SPAWN_EARTH_PET_SERVER.lua", []() {return std::make_unique<NjEarthDragonPetServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_EARTH_PET_SERVER.lua", []() {return std::make_unique<NjEarthPetServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_DRAGON_EMBLEM_CHEST_SERVER.lua", []() {return std::make_unique<NjDragonEmblemChestServer>();}},
		{"scripts\\02_server\\Map\\njhub\\L_NYA_MISSION_ITEMS.lua", []() {return std::make_unique<NjNyaMissionitems>();}},

		//DLU
		{"scripts\\02_server\\DLU\\DLUVanityTeleportingObject.lua", []() {return std::make_unique<DLUVanityTeleportingObject>();}},

		//Survival Minigame
		{"scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_STROMBIE.lua", []() {return std::make_unique<AgSurvivalStromling>();}},
		{"scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARKLING_MECH.lua", []() {return std::make_unique<AgSurvivalMech>();}},
		{"scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARK_SPIDERLING.lua", []() {return std::make_unique<AgSurvivalSpiderling>();}},

		//Scripted Equipment
		{"scripts\\EquipmentScripts\\Sunflower.lua", []() {return std::make_unique<Sunflower>();}},
		{"scripts/EquipmentScripts/AnvilOfArmor.lua", []() {return std::make_unique<AnvilOfArmor>();}},
		{"scripts/EquipmentScripts/FountainOfImagination.lua", []() {return std::make_unique<FountainOfImagination>();}},
		{"scripts/EquipmentScripts/CauldronOfLife.lua", []() {return std::make_unique<CauldronOfLife>();}},
		{"scripts\\02_server\\Equipment\\L_BOOTYDIG_SERVER.lua", []() {return std::make_unique<BootyDigServer>();}},
		{"scripts\\EquipmentScripts\\PersonalFortress.lua", []() {return std::make_unique<PersonalFortress>();}},
		{"scripts\\02_server\\Map\\General\\L_PROPERTY_DEVICE.lua", []() {return std::make_unique<PropertyDevice>();}},
		{"scripts\\02_server\\Map\\General\\L_IMAG_BACKPACK_HEALS_SERVER.lua", []() {return std::make_unique<ImaginationBackpackHealServer>();}},
		{"scripts\\ai\\GENERAL\\L_LEGO_DIE_ROLL.lua", []() {return std::make_unique<LegoDieRoll>();}},
		{"scripts\\EquipmentScripts\\BuccaneerValiantShip.lua", []() {return std::make_unique<BuccaneerValiantShip>();}},
		{"scripts\\EquipmentScripts\\FireFirstSkillonStartup.lua", []() {return std::make_unique<FireFirstSkillonStartup>();}},
		{"scripts\\equipmenttriggers\\gempack.lua", []() {return std::make_unique<GemPack>();}},
		{"scripts\\equipmenttriggers\\shardarmor.lua", []() {return std::make_unique<ShardArmor>();}},
		{"scripts\\equipmenttriggers\\coilbackpack.lua", []() {return std::make_unique<TeslaPack>();}},
		{"scripts\\EquipmentScripts\\stunImmunity.lua", []() {return std::make_unique<StunImmunity>();}},

		//FB
		{"scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_BROKEN.lua", []() {return std::make_unique<RockHydrantBroken>();}},
		{"scripts\\ai\\NS\\L_NS_WH_FANS.lua", []() {return std::make_unique<WhFans>();}},

		//WBL
		{"scripts\\zone\\LUPs\\WBL_generic_zone.lua", []() {return std::make_unique<WblGenericZone>();}},

		//Alpha
		{"scripts\\ai\\FV\\L_TRIGGER_GAS.lua", []() {return std::make_unique<TriggerGas>();}},
		{"scripts\\ai\\FV\\L_ACT_NINJA_SENSEI.lua", []() {return std::make_unique<ActNinjaSensei>();}},

		//Pickups
		{"scripts\\ai\\SPEC\\L_SPECIAL_1_BRONZE-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(1);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_1_GOLD-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(10000);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_1_SILVER-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(100);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_10_BRONZE-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(10);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_10_GOLD-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(100000);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_10_SILVER-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(1000);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_25_BRONZE-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(25);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_25_GOLD-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(250000);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_25_SILVER-COIN-SPAWNER.lua", []() {return std::make_unique<SpecialCoinSpawner>(2500);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER.lua", []() {return std::make_unique<SpecialPowerupSpawner>(13);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER-2PT.lua", []() {return std::make_unique<SpecialPowerupSpawner>(129);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_LIFE-POWERUP-SPAWNER.lua", []() {return std::make_unique<SpecialPowerupSpawner>(5);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_ARMOR-POWERUP-SPAWNER.lua", []() {return std::make_unique<SpecialPowerupSpawner>(747);}},
		{"scripts\\ai\\SPEC\\L_SPECIAL_SPEED_BUFF_SPAWNER.lua", []() {return std::make_unique<SpecialSpeedBuffSpawner>();}},

		//Wild
		{"scripts\\ai\\WILD\\L_WILD_GF_RAT.lua", []() {return std::make_unique<WildAndScared>();}},
		{"scripts\\ai\\WILD\\L_WILD_GF_SNAIL.lua", []() {return std::make_unique<WildAndScared>();}},
		{"scripts\\ai\\WILD\\L_WILD_GF_GLOWBUG.lua", []() {return std::make_unique<WildGfGlowbug>();}},
		{"scripts\\ai\\WILD\\L_WILD_AMBIENT_CRAB.lua", []() {return std::make_unique<WildAmbientCrab>();}},
		{"scripts\\ai\\WILD\\L_WILD_PANTS.lua", []() {return std::make_unique<WildPants>();}},
		{"scripts\\ai\\WILD\\L_WILD_NINJA_BRICKS.lua", []() {return std::make_unique<WildNinjaBricks>();}},
		{"scripts\\ai\\WILD\\L_WILD_NINJA_STUDENT.lua", []() {return std::make_unique<WildNinjaStudent>();}},
		{"scripts\\ai\\WILD\\L_WILD_NINJA_SENSEI.lua", []() {return std::make_unique<WildNinjaSensei>();}},
		{"scripts\\ai\\WILD\\L_LUP_generic_interact.lua", []() {return std::make_unique<LupGenericInteract>();}},
		{"scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenBlue.lua", []() {return std::make_unique<WblRobotCitizen>();}},
		{"scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenGreen.lua", []() {return std::make_unique<WblRobotCitizen>();}},
		{"scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenOrange.lua", []() {return std::make_unique<WblRobotCitizen>();}},
		{"scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenRed.lua", []() {return std::make_unique<WblRobotCitizen>();}},
		{"scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenYellow.lua", []() {return std::make_unique<WblRobotCitizen>();}},
		{"scripts\\02_server\\Map\\General\\L_ENEMY_CLEAR_THREAT.lua", []() {return std::make_unique<EnemyClearThreat>();}},
		{"scripts\\ai\\AG\\L_AG_SPIDER_BOSS_MESSAGE.lua", []() {return std::make_unique<AgSpiderBossMessage>();}},

	};

	std::set<std::string> g_ExcludedScripts = {
		"scripts\\02_server\\Enemy\\General\\L_SUSPEND_LUA_AI.lua",
		"scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_SPIDERLING.lua",
		"scripts\\ai\\AG\\L_AG_SENTINEL_GUARD.lua",
		"scripts\\ai\\FV\\L_ACT_NINJA_STUDENT.lua",
		"scripts\\ai\\WILD\\L_WILD_GF_FROG.lua",
		"scripts\\empty.lua",
		"scripts\\zone\\AG\\L_ZONE_AG.lua",
		"scripts\\zone\\NS\\L_ZONE_NS.lua",
		"scripts\\zone\\GF\\L_ZONE_GF.lua",
	};
};

CppScripts::Script& CppScripts::GetScript(Entity* parent, const std::string& scriptName) {
	auto itr = g_Scripts.find(scriptName);
	if (itr != g_Scripts.end()) {
		return *itr->second;
	}

	const auto itrTernary = scriptLoader.find(scriptName);
	auto& script = itrTernary != scriptLoader.cend() ? *itrTernary->second() : InvalidToReturn;

	if (&script == &InvalidToReturn && !scriptName.empty() && !g_ExcludedScripts.contains(scriptName)) {
		LOG_DEBUG("LOT %i attempted to load CppScript for '%s', but returned InvalidScript.", parent->GetLOT(), scriptName.c_str());
	}

	g_Scripts[scriptName] = &script;
	return script;
}

CppScripts::Script& CppScripts::GetInvalidScript() {
	return InvalidToReturn;
}
