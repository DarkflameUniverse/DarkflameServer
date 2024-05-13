#include <gtest/gtest.h>
#include "dCommonVars.h"
#include "eCinematicEvent.h"
#include "CppScripts.h"
#include "CppScriptsOld.h"
#include "Entity.h"
#include "EntityInfo.h"
#include "LupGenericInteract.h"
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

#define CHECK_SCRIPT(scriptName, scriptType) do { \
	auto* script = dynamic_cast<scriptType*>(CppScripts::GetScript(&entity, scriptName)); \
	auto* oldScript = dynamic_cast<scriptType*>(CppScriptsOld::GetScript(&entity, scriptName)); \
	ASSERT_NE(script, nullptr); \
	ASSERT_NE(oldScript, nullptr); \
} while (0)

#define CHECK_CACHE(scriptName) do { \
	ASSERT_EQ(CppScripts::GetScript(&entity, scriptName), CppScripts::GetScript(&entity, scriptName)); \
} while (0)

TEST(dScriptsTests, OldCppScriptsCheck) {
	Entity entity(LWOOBJID_EMPTY, EntityInfo{});
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_LUP_generic_interact.lua", LupGenericInteract);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_SHIP_PLAYER_DEATH_TRIGGER.lua", AgShipPlayerDeathTrigger);
	CHECK_SCRIPT("scripts\\ai\\NP\\L_NPC_NP_SPACEMAN_BOB.lua", NpcNpSpacemanBob);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_SPACE_STUFF.lua", AgSpaceStuff);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_SHIP_PLAYER_SHOCK_SERVER.lua", AgShipPlayerShockServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_IMAG_SMASHABLE.lua", AgImagSmashable);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_STORY_BOX_INTERACT_SERVER.lua", StoryBoxInteractServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_BINOCULARS.lua", Binoculars);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_ALL_CRATE_CHICKEN.lua", AllCrateChicken);
	CHECK_SCRIPT("scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_SMASHABLE.lua", RockHydrantSmashable);
	CHECK_SCRIPT("scripts\\02_server\\Map\\SS\\L_SS_MODULAR_BUILD_SERVER.lua", SsModularBuildServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\Property\\AG_Small\\L_ZONE_AG_PROPERTY.lua", ZoneAgProperty);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_POI_MISSION.lua", InvalidScript);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_TOUCH_MISSION_UPDATE_SERVER.lua", TouchMissionUpdateServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_ACT_SHARK_PLAYER_DEATH_TRIGGER.lua", ActSharkPlayerDeathTrigger);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_MECH.lua", BaseEnemyMech);
	CHECK_SCRIPT("scripts\\zone\\AG\\L_ZONE_AG_SURVIVAL.lua", ZoneAgSurvival);
	CHECK_SCRIPT("scripts\\02_server\\Objects\\L_BUFF_STATION_SERVER.lua", AgSurvivalBuffStation);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_BUS_DOOR.lua", AgBusDoor);
	CHECK_SCRIPT("scripts\\02_server\\Equipment\\L_MAESTROM_EXTRACTICATOR_SERVER.lua", MaestromExtracticatorServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_AG_CAGED_BRICKS_SERVER.lua", AgCagedBricksServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_WISP_SERVER.lua", NpcWispServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_EPSILON_SERVER.lua", NpcEpsilonServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_TURRET.lua", AgTurret);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_TURRET_FOR_SHIP.lua", AgTurret);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_AG_LASER_SENSOR_SERVER.lua", AgLaserSensorServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_LASER_SERVER.lua", AgMonumentLaserServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_FANS.lua", AgFans);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_BIRDS.lua", AgMonumentBirds);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_REMOVE_RENTAL_GEAR.lua", RemoveRentalGear);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_NJ_ASSISTANT_SERVER.lua", NpcNjAssistantServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_SALUTING_NPCS.lua", AgSalutingNpcs);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_JET_EFFECT_SERVER.lua", AgJetEffectServer);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\AG\\L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua", BossSpiderQueenEnemyServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\Property\\AG_Small\\L_ENEMY_SPIDER_SPAWNER.lua", EnemySpiderSpawner);
	CHECK_SCRIPT("scripts/02_server/Map/Property/AG_Small/L_ENEMY_SPIDER_SPAWNER.lua", EnemySpiderSpawner);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_QB_Elevator.lua", AgQbElevator);
	CHECK_SCRIPT("scripts\\ai\\PROPERTY\\AG\\L_AG_PROP_GUARD.lua", AgPropGuard);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_AG_BUGSPRAYER.lua", AgBugsprayer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_AG_COURSE_STARTER.lua", NpcAgCourseStarter);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_GOAL.lua", AgMonumentRaceGoal);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_CANCEL.lua", AgMonumentRaceCancel);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG_Spider_Queen\\L_ZONE_AG_SPIDER_QUEEN.lua", ZoneAgSpiderQueen);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG_Spider_Queen\\L_SPIDER_BOSS_TREASURE_CHEST_SERVER.lua", SpiderBossTreasureChestServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_COWBOY_SERVER.lua", NpcCowboyServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\Property\\AG_Med\\L_ZONE_AG_MED_PROPERTY.lua", ZoneAgMedProperty);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_STROMBIE_PROPERTY.lua", AgStromlingProperty);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_DARKLING_MECH.lua", BaseEnemyMech);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_DARK_SPIDERLING.lua", AgDarkSpiderling);
	CHECK_SCRIPT("scripts\\ai\\PROPERTY\\L_PROP_GUARDS.lua", AgPropguards);
	CHECK_SCRIPT("scripts\\ai\\PROPERTY\\L_PROPERTY_FX_DAMAGE.lua", PropertyFXDamage);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AG\\L_NPC_PIRATE_SERVER.lua", NpcPirateServer);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_PICNIC_BLANKET.lua", AgPicnicBlanket);
	CHECK_SCRIPT("scripts\\02_server\\Map\\Property\\L_PROPERTY_BANK_INTERACT_SERVER.lua", PropertyBankInteract);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\VE\\L_VE_MECH.lua", VeMech);
	CHECK_SCRIPT("scripts\\02_server\\Map\\VE\\L_MISSION_CONSOLE_SERVER.lua", VeMissionConsole);
	CHECK_SCRIPT("scripts\\02_server\\Map\\VE\\L_EPSILON_SERVER.lua", VeEpsilonServer);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_MODULAR_BUILD.lua", NsModularBuild);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_GET_FACTION_MISSION_SERVER.lua", NsGetFactionMissionServer);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_QB_IMAGINATION_STATUE.lua", NsQbImaginationStatue);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NS\\CONCERT_CHOICEBUILD_MANAGER_SERVER.lua", NsConcertChoiceBuildManager);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_CONCERT_CHOICEBUILD.lua", NsConcertChoiceBuild);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_CONCERT_QUICKBUILD.lua", NsConcertQuickBuild);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_STAGE_PLATFORMS.lua", AgStagePlatforms);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_CONCERT_INSTRUMENT_QB.lua", NsConcertInstrument);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_JONNY_FLAG_MISSION_SERVER.lua", NsJohnnyMissionServer);
	CHECK_SCRIPT("scripts\\02_server\\Objects\\L_STINKY_FISH_TARGET.lua", StinkyFishTarget);
	CHECK_SCRIPT("scripts\\zone\\PROPERTY\\NS\\L_ZONE_NS_PROPERTY.lua", ZoneNsProperty);
	CHECK_SCRIPT("scripts\\02_server\\Map\\Property\\NS_Med\\L_ZONE_NS_MED_PROPERTY.lua", ZoneNsMedProperty);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NS\\L_NS_TOKEN_CONSOLE_SERVER.lua", NsTokenConsoleServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NS\\L_NS_LUP_TELEPORT.lua", NsLupTeleport);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NS\\Waves\\L_ZONE_NS_WAVES.lua", ZoneNsWaves);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HAMMERLING_ENEMY_SERVER.lua", WaveBossHammerling);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_APE_ENEMY_SERVER.lua", WaveBossApe);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_DARK_SPIDERLING_ENEMY_SERVER.lua", WaveBossSpiderling);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HORESEMEN_ENEMY_SERVER.lua", WaveBossHorsemen);
	CHECK_SCRIPT("scripts\\02_server\\Minigame\\General\\L_MINIGAME_TREASURE_CHEST_SERVER.lua", MinigameTreasureChestServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NS\\L_NS_LEGO_CLUB_DOOR.lua", NsLegoClubDoor);
	CHECK_SCRIPT("scripts/ai/NS/L_CL_RING.lua", ClRing);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_AMBIENTS.lua", WildAmbients);
	CHECK_SCRIPT("scripts\\ai\\NS\\NS_PP_01\\L_NS_PP_01_TELEPORT.lua", PropertyDeathPlane);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_QB_SPAWNER.lua", QbSpawner);
	CHECK_SCRIPT("scripts\\ai\\AG\\L_AG_QB_Wall.lua", AgQbWall);
	CHECK_SCRIPT("scripts\\02_server\\Map\\GF\\L_GF_TORCH.lua", GfTikiTorch);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_SPECIAL_FIREPIT.lua", GfCampfire);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_ORGAN.lua", GfOrgan);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_BANANA.lua", GfBanana);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_BANANA_CLUSTER.lua", GfBananaCluster);
	CHECK_SCRIPT("scripts/ai/GF/L_GF_JAILKEEP_MISSION.lua", GfJailkeepMission);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_TRIGGER_AMBUSH.lua", TriggerAmbush);
	CHECK_SCRIPT("scripts\\02_server\\Map\\GF\\L_GF_CAPTAINS_CANNON.lua", GfCaptainsCannon);
	CHECK_SCRIPT("scripts\\02_server\\Map\\GF\\L_MAST_TELEPORT.lua", MastTeleport);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_JAIL_WALLS.lua", GfJailWalls);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_QB_ENEMY_STUNNER.lua", QbEnemyStunner);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_PET_DIG_BUILD.lua", PetDigBuild);
	CHECK_SCRIPT("scripts\\02_server\\Map\\GF\\L_SPAWN_LION_SERVER.lua", SpawnLionServer);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_APE.lua", BaseEnemyApe);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_GF_APE_SMASHING_QB.lua", GfApeSmashingQB);
	CHECK_SCRIPT("scripts\\zone\\PROPERTY\\GF\\L_ZONE_GF_PROPERTY.lua", ZoneGfProperty);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_ARCHWAY.lua", GfArchway);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_MAELSTROM_GEYSER.lua", GfMaelstromGeyser);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_PIRATE_REP.lua", PirateRep);
	CHECK_SCRIPT("scripts\\ai\\GF\\L_GF_PARROT_CRASH.lua", GfParrotCrash);
	CHECK_SCRIPT("scripts\\ai\\MINIGAME\\SG_GF\\SERVER\\SG_CANNON.lua", SGCannon);
	CHECK_SCRIPT("scripts\\ai\\MINIGAME\\SG_GF\\L_ZONE_SG_SERVER.lua", ZoneSGServer);
	CHECK_SCRIPT("scripts\\client\\ai\\PR\\L_PR_WHISTLE.lua", PrWhistle);
	CHECK_SCRIPT("scripts\\02_server\\Map\\PR\\L_PR_SEAGULL_FLY.lua", PrSeagullFly);
	CHECK_SCRIPT("scripts\\ai\\PETS\\L_HYDRANT_SMASHABLE.lua", HydrantSmashable);
	CHECK_SCRIPT("scripts\\02_server\\map\\PR\\L_HYDRANT_BROKEN.lua", HydrantBroken);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\PET_DIG_SERVER.lua", PetDigServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SKELETON_DRAGON_PET_DIG_SERVER.lua", PetDigServer);
	CHECK_SCRIPT("scripts\\client\\ai\\PR\\L_CRAB_SERVER.lua", CrabServer);
	CHECK_SCRIPT("scripts\\02_server\\Pets\\L_PET_FROM_DIG_SERVER.lua", PetFromDigServer);
	CHECK_SCRIPT("scripts\\02_server\\Pets\\L_PET_FROM_OBJECT_SERVER.lua", PetFromObjectServer);
	CHECK_SCRIPT("scripts\\02_server\\Pets\\L_DAMAGING_PET.lua", DamagingPets);
	CHECK_SCRIPT("scripts\\02_server\\Map\\PR\\L_SPAWN_GRYPHON_SERVER.lua", SpawnGryphonServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\L_ACT_CANDLE.lua", FvCandle);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\L_ENEMY_RONIN_SPAWNER.lua", EnemyRoninSpawner);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_CAVALRY.lua", FvMaelstromCavalry);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_ACT_NINJA_TURRET_1.lua", ActNinjaTurret);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\L_FV_HORSEMEN_TRIGGER.lua", FvHorsemenTrigger);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_FLYING_CREVICE_DRAGON.lua", FvFlyingCreviceDragon);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_DRAGON.lua", FvMaelstromDragon);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_DRAGON_SMASHING_GOLEM_QB.lua", FvDragonSmashingGolemQb);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_TREASURE_CHEST_DRAGON_SERVER.lua", TreasureChestDragonServer);
	CHECK_SCRIPT("scripts\\ai\\GENERAL\\L_INSTANCE_EXIT_TRANSFER_PLAYER_TO_LAST_NON_INSTANCE.lua", InstanceExitTransferPlayerToLastNonInstance);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_NPC_FREE_GF_NINJAS.lua", FvFreeGfNinjas);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_PANDA_SPAWNER_SERVER.lua", FvPandaSpawnerServer);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_PANDA_SERVER.lua", FvPandaServer);
	CHECK_SCRIPT("scripts\\zone\\PROPERTY\\FV\\L_ZONE_FV_PROPERTY.lua", ZoneFvProperty);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_BRICK_PUZZLE_SERVER.lua", FvBrickPuzzleServer);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_CONSOLE_LEFT_QUICKBUILD.lua", FvConsoleLeftQuickbuild);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_CONSOLE_RIGHT_QUICKBUILD.lua", FvConsoleRightQuickbuild);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_FACILITY_BRICK.lua", FvFacilityBrick);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_FACILITY_PIPES.lua", FvFacilityPipes);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\L_IMG_BRICK_CONSOLE_QB.lua", ImgBrickConsoleQB);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_ACT_PARADOX_PIPE_FIX.lua", ActParadoxPipeFix);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_NINJA_GUARDS.lua", FvNinjaGuard);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_ACT_PASS_THROUGH_WALL.lua", FvPassThroughWall);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_ACT_BOUNCE_OVER_WALL.lua", FvBounceOverWall);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\L_NPC_FONG.lua", FvFong);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_FV_MAELSTROM_GEYSER.lua", FvMaelstromGeyser);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\Racing\\RACE_SHIP_LAP_COLUMNS_SERVER.lua", RaceShipLapColumnsServer);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP1_SERVER.lua", FvRaceDragon);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP2_SERVER.lua", FvRaceDragon);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP3_SERVER.lua", FvRaceDragon);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_ABC_SERVER.lua", FvRacePillarABCServer);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_D_SERVER.lua", FvRacePillarDServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\Racing\\RACE_FIREBALLS.lua", RaceFireballs);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_EXPLODING_ASSET.lua", ExplodingAsset);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_WISHING_WELL_SERVER.lua", WishingWellServer);
	CHECK_SCRIPT("scripts\\ai\\ACT\\L_ACT_PLAYER_DEATH_TRIGGER.lua", ActPlayerDeathTrigger);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_GROWING_FLOWER_SERVER.lua", GrowingFlower);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_TOKEN_CONSOLE_SERVER.lua", TokenConsoleServer);
	CHECK_SCRIPT("scripts\\ai\\ACT\\FootRace\\L_ACT_BASE_FOOT_RACE.lua", BaseFootRaceManager);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_PROP_PLATFORM.lua", PropertyPlatform);
	CHECK_SCRIPT("scripts\\02_server\\Map\\VE\\L_VE_BRICKSAMPLE_SERVER.lua", VeBricksampleServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_MAIL_BOX_SERVER.lua", MailBoxServer);
	CHECK_SCRIPT("scripts\\ai\\ACT\\L_ACT_MINE.lua", ActMine);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_WANDERING_VENDOR.lua", WanderingVendor);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_CRATE_SERVER.lua", RaceImagineCrateServer);
	CHECK_SCRIPT("scripts\\ai\\ACT\\L_ACT_VEHICLE_DEATH_TRIGGER.lua", ActVehicleDeathTrigger);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_POWERUP.lua", RaceImaginePowerup);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\Racing\\RACE_MAELSTROM_GEISER.lua", RaceMaelstromGeiser);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_SMASH_EGG_IMAGINE_SERVER.lua", FvRaceSmashEggImagineServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\FV\\Racing\\FV_RACING_COLUMNS.lua", FvRacingColumns);
	CHECK_SCRIPT("scripts\\ai\\RACING\\OBJECTS\\RACE_SMASH_SERVER.lua", RaceSmashServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_SENTINELWALKWAY_SERVER.lua", NtSentinelWalkwayServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_PARADOXTELE_SERVER.lua", NtParadoxTeleServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_DARKITECT_REVEAL_SERVER.lua", NtDarkitectRevealServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_BANK_INTERACT_SERVER.lua", BankInteractServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_VENTURESPEEDPAD_SERVER.lua", NtVentureSpeedPadServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_VENTURE_CANNON_SERVER.lua", NtVentureCannonServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_SERVER.lua", NtCombatChallengeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_DUMMY.lua", NtCombatChallengeDummy);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\\\L_NT_COMBAT_EXPLODING_TARGET.lua", NtCombatChallengeExplodingDummy);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_BASE_INTERACT_DROP_LOOT_SERVER.lua", BaseInteractDropLootServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_ASSEMBLYTUBE_SERVER.lua", NtAssemblyTubeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_PARADOX_PANEL_SERVER.lua", NtParadoxPanelServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_IMAG_BEAM_BUFFER.lua", NtImagBeamBuffer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_BEAM_IMAGINATION_COLLECTORS.lua", NtBeamImaginationCollectors);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_DIRT_CLOUD_SERVER.lua", NtDirtCloudServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_CONSOLE_TELEPORT_SERVER.lua", NtConsoleTeleportServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_SPAWN_STEGO_SERVER.lua", SpawnStegoServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_SPAWN_SABERCAT_SERVER.lua", SpawnSaberCatServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_SPAWN_SHRAKE_SERVER.lua", SpawnShrakeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_DUKE_SERVER.lua", NtDukeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_HAEL_SERVER.lua", NtHaelServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_FACTION_SPY_SERVER.lua", NtFactionSpyServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_OVERBUILD_SERVER.lua", NtOverbuildServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_VANDA_SERVER.lua", NtVandaServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_FORCE_VOLUME_SERVER.lua", ForceVolumeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_FRICTION_VOLUME_SERVER.lua", FrictionVolumeServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_XRAY_SERVER.lua", NtXRayServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_SLEEPING_GUARD.lua", NtSleepingGuard);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_IMAGIMETER_VISIBILITY_SERVER.lua", NTImagimeterVisibility);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_PIPE_VISIBILITY_SERVER.lua", NTPipeVisibilityServer);
	CHECK_SCRIPT("scripts\\ai\\MINIGAME\\Objects\\MINIGAME_BLUE_MARK.lua", MinigameBlueMark);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_NAOMI_BREADCRUMB_SERVER.lua", NtNaomiBreadcrumbServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\NT\\L_NT_NAOMI_DIRT_SERVER.lua", NTNaomiDirtServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_AM_CONSOLE_TELEPORT_SERVER.lua", AmConsoleTeleportServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_FIN.lua", RandomSpawnerFin);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_PIT.lua", RandomSpawnerPit);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_STR.lua", RandomSpawnerStr);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_ZIP.lua", RandomSpawnerZip);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_MECH.lua", AmDarklingMech);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_BRIDGE.lua", AmBridge);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_DRAW_BRIDGE.lua", AmDrawBridge);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR.lua", AmShieldGenerator);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR_QUICKBUILD.lua", AmShieldGeneratorQuickbuild);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_DROPSHIP_COMPUTER.lua", AmDropshipComputer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SCROLL_READER_SERVER.lua", AmScrollReaderServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_TEMPLE_SKILL_VOLUME.lua", AmTemplateSkillVolume);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF.lua", EnemyNjBuff);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\AM\\L_AM_SKELETON_ENGINEER.lua", AmSkeletonEngineer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL.lua", AmSkullkinDrill);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL_STAND.lua", AmSkullkinDrillStand);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_SKULLKIN_TOWER.lua", AmSkullkinTower);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\AM\\L_AM_NAMED_DARKLING_DRAGON.lua", AmDarklingDragon);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_APE.lua", BaseEnemyApe);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_BLUE_X.lua", AmBlueX);
	CHECK_SCRIPT("scripts\\02_server\\Map\\AM\\L_TEAPOT_SERVER.lua", AmTeapotServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_GARMADON_CELEBRATION_SERVER.lua", NjGarmadonCelebration);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_WU_NPC.lua", NjWuNPC);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_SCROLL_CHEST_SERVER.lua", NjScrollChestServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_COLE_NPC.lua", NjColeNPC);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_JAY_MISSION_ITEMS.lua", NjJayMissionItems);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_NPC_MISSION_SPINJITZU_SERVER.lua", NjNPCMissionSpinjitzuServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_ENEMY_SKELETON_SPAWNER.lua", EnemySkeletonSpawner);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_NJ_RAIL_SWITCH.lua", NjRailSwitch);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_ACTIVATORS_SERVER.lua", NjRailActivatorsServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_POST_SERVER.lua", NjRailPostServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\Ninjago\\L_ICE_RAIL_ACTIVATOR_SERVER.lua", NjIceRailActivator);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_FALLING_TILE.lua", FallingTile);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF_STUN_IMMUNITY.lua", EnemyNjBuff);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_IMAGINATION_SHRINE_SERVER.lua", ImaginationShrineServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_LIEUTENANT.lua", Lieutenant);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_RAIN_OF_ARROWS.lua", RainOfArrows);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_CAVE_PRISON_CAGE.lua", CavePrisonCage);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\boss_instance\\L_MONASTERY_BOSS_INSTANCE_SERVER.lua", NjMonastryBossInstance);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_CATAPULT_BOUNCER_SERVER.lua", CatapultBouncerServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_CATAPULT_BASE_SERVER.lua", CatapultBaseServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\Ninjago\\L_NJHUB_LAVA_PLAYER_DEATH_TRIGGER.lua", NjhubLavaPlayerDeathTrigger);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_MON_CORE_NOOK_DOORS.lua", MonCoreNookDoors);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua", MonCoreSmashableDoors);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua", MonCoreSmashableDoors);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_FLAME_JET_SERVER.lua", FlameJetServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_BURNING_TILE.lua", BurningTile);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_SPAWN_EARTH_PET_SERVER.lua", NjEarthDragonPetServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_EARTH_PET_SERVER.lua", NjEarthPetServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_DRAGON_EMBLEM_CHEST_SERVER.lua", NjDragonEmblemChestServer);
	CHECK_SCRIPT("scripts\\02_server\\Map\\njhub\\L_NYA_MISSION_ITEMS.lua", NjNyaMissionitems);
	CHECK_SCRIPT("scripts\\02_server\\DLU\\DLUVanityTeleportingObject.lua", DLUVanityTeleportingObject);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_STROMBIE.lua", AgSurvivalStromling);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARKLING_MECH.lua", AgSurvivalMech);
	CHECK_SCRIPT("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARK_SPIDERLING.lua", AgSurvivalSpiderling);
	CHECK_SCRIPT("scripts\\EquipmentScripts\\Sunflower.lua", Sunflower);
	CHECK_SCRIPT("scripts/EquipmentScripts/AnvilOfArmor.lua", AnvilOfArmor);
	CHECK_SCRIPT("scripts/EquipmentScripts/FountainOfImagination.lua", FountainOfImagination);
	CHECK_SCRIPT("scripts/EquipmentScripts/CauldronOfLife.lua", CauldronOfLife);
	CHECK_SCRIPT("scripts\\02_server\\Equipment\\L_BOOTYDIG_SERVER.lua", BootyDigServer);
	CHECK_SCRIPT("scripts\\EquipmentScripts\\PersonalFortress.lua", PersonalFortress);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_PROPERTY_DEVICE.lua", PropertyDevice);
	CHECK_SCRIPT("scripts\\02_server\\Map\\General\\L_IMAG_BACKPACK_HEALS_SERVER.lua", ImaginationBackpackHealServer);
	CHECK_SCRIPT("scripts\\ai\\GENERAL\\L_LEGO_DIE_ROLL.lua", LegoDieRoll);
	CHECK_SCRIPT("scripts\\EquipmentScripts\\BuccaneerValiantShip.lua", BuccaneerValiantShip);
	CHECK_SCRIPT("scripts\\EquipmentScripts\\FireFirstSkillonStartup.lua", FireFirstSkillonStartup);
	CHECK_SCRIPT("scripts\\equipmenttriggers\\gempack.lua", GemPack);
	CHECK_SCRIPT("scripts\\equipmenttriggers\\shardarmor.lua", ShardArmor);
	CHECK_SCRIPT("scripts\\equipmenttriggers\\coilbackpack.lua", TeslaPack);
	CHECK_SCRIPT("scripts\\EquipmentScripts\\stunImmunity.lua", StunImmunity);
	CHECK_SCRIPT("scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_BROKEN.lua", RockHydrantBroken);
	CHECK_SCRIPT("scripts\\ai\\NS\\L_NS_WH_FANS.lua", WhFans);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\WBL_generic_zone.lua", WblGenericZone);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_TRIGGER_GAS.lua", TriggerGas);
	CHECK_SCRIPT("scripts\\ai\\FV\\L_ACT_NINJA_SENSEI.lua", ActNinjaSensei);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_1_BRONZE-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_1_SILVER-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_10_BRONZE-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_10_GOLD-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_10_SILVER-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_25_BRONZE-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_25_GOLD-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_25_SILVER-COIN-SPAWNER.lua", SpecialCoinSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER.lua", SpecialPowerupSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER-2PT.lua", SpecialPowerupSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_LIFE-POWERUP-SPAWNER.lua", SpecialPowerupSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_ARMOR-POWERUP-SPAWNER.lua", SpecialPowerupSpawner);
	CHECK_SCRIPT("scripts\\ai\\SPEC\\L_SPECIAL_SPEED_BUFF_SPAWNER.lua", SpecialSpeedBuffSpawner);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_GF_RAT.lua", WildAndScared);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_GF_SNAIL.lua", WildAndScared);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_GF_GLOWBUG.lua", WildGfGlowbug);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_AMBIENT_CRAB.lua", WildAmbientCrab);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_PANTS.lua", WildPants);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_NINJA_BRICKS.lua", WildNinjaBricks);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_NINJA_STUDENT.lua", WildNinjaStudent);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_WILD_NINJA_SENSEI.lua", WildNinjaSensei);
	CHECK_SCRIPT("scripts\\ai\\WILD\\L_LUP_generic_interact.lua", LupGenericInteract);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenBlue.lua", WblRobotCitizen);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenGreen.lua", WblRobotCitizen);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenOrange.lua", WblRobotCitizen);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenRed.lua", WblRobotCitizen);
	CHECK_SCRIPT("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenYellow.lua", WblRobotCitizen);
}

TEST(dScriptsTests, CacheCheck) {
	Entity entity(LWOOBJID_EMPTY, EntityInfo{});
	CHECK_CACHE("scripts\\ai\\WILD\\L_LUP_generic_interact.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_SHIP_PLAYER_DEATH_TRIGGER.lua");
	CHECK_CACHE("scripts\\ai\\NP\\L_NPC_NP_SPACEMAN_BOB.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_SPACE_STUFF.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_SHIP_PLAYER_SHOCK_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_IMAG_SMASHABLE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_STORY_BOX_INTERACT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_BINOCULARS.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_ALL_CRATE_CHICKEN.lua");
	CHECK_CACHE("scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_SMASHABLE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\SS\\L_SS_MODULAR_BUILD_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\Property\\AG_Small\\L_ZONE_AG_PROPERTY.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_POI_MISSION.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_TOUCH_MISSION_UPDATE_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_ACT_SHARK_PLAYER_DEATH_TRIGGER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_MECH.lua");
	CHECK_CACHE("scripts\\zone\\AG\\L_ZONE_AG_SURVIVAL.lua");
	CHECK_CACHE("scripts\\02_server\\Objects\\L_BUFF_STATION_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_BUS_DOOR.lua");
	CHECK_CACHE("scripts\\02_server\\Equipment\\L_MAESTROM_EXTRACTICATOR_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_AG_CAGED_BRICKS_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_WISP_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_EPSILON_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_TURRET.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_TURRET_FOR_SHIP.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_AG_LASER_SENSOR_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_LASER_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_FANS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_AG_MONUMENT_BIRDS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_REMOVE_RENTAL_GEAR.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_NJ_ASSISTANT_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_SALUTING_NPCS.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_JET_EFFECT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\AG\\L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\Property\\AG_Small\\L_ENEMY_SPIDER_SPAWNER.lua");
	CHECK_CACHE("scripts/02_server/Map/Property/AG_Small/L_ENEMY_SPIDER_SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_QB_Elevator.lua");
	CHECK_CACHE("scripts\\ai\\PROPERTY\\AG\\L_AG_PROP_GUARD.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_AG_BUGSPRAYER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_AG_COURSE_STARTER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_GOAL.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L__AG_MONUMENT_RACE_CANCEL.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG_Spider_Queen\\L_ZONE_AG_SPIDER_QUEEN.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG_Spider_Queen\\L_SPIDER_BOSS_TREASURE_CHEST_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_COWBOY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\Property\\AG_Med\\L_ZONE_AG_MED_PROPERTY.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_STROMBIE_PROPERTY.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_DARKLING_MECH.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_DARK_SPIDERLING.lua");
	CHECK_CACHE("scripts\\ai\\PROPERTY\\L_PROP_GUARDS.lua");
	CHECK_CACHE("scripts\\ai\\PROPERTY\\L_PROPERTY_FX_DAMAGE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AG\\L_NPC_PIRATE_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_PICNIC_BLANKET.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\Property\\L_PROPERTY_BANK_INTERACT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\VE\\L_VE_MECH.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\VE\\L_MISSION_CONSOLE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\VE\\L_EPSILON_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_MODULAR_BUILD.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_GET_FACTION_MISSION_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_QB_IMAGINATION_STATUE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NS\\CONCERT_CHOICEBUILD_MANAGER_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_CONCERT_CHOICEBUILD.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_CONCERT_QUICKBUILD.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_STAGE_PLATFORMS.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_CONCERT_INSTRUMENT_QB.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_JONNY_FLAG_MISSION_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Objects\\L_STINKY_FISH_TARGET.lua");
	CHECK_CACHE("scripts\\zone\\PROPERTY\\NS\\L_ZONE_NS_PROPERTY.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\Property\\NS_Med\\L_ZONE_NS_MED_PROPERTY.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NS\\L_NS_TOKEN_CONSOLE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NS\\L_NS_LUP_TELEPORT.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NS\\Waves\\L_ZONE_NS_WAVES.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HAMMERLING_ENEMY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_APE_ENEMY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_DARK_SPIDERLING_ENEMY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Waves\\L_WAVES_BOSS_HORESEMEN_ENEMY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Minigame\\General\\L_MINIGAME_TREASURE_CHEST_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NS\\L_NS_LEGO_CLUB_DOOR.lua");
	CHECK_CACHE("scripts/ai/NS/L_CL_RING.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_AMBIENTS.lua");
	CHECK_CACHE("scripts\\ai\\NS\\NS_PP_01\\L_NS_PP_01_TELEPORT.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_QB_SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\AG\\L_AG_QB_Wall.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\GF\\L_GF_TORCH.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_SPECIAL_FIREPIT.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_ORGAN.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_BANANA.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_BANANA_CLUSTER.lua");
	CHECK_CACHE("scripts/ai/GF/L_GF_JAILKEEP_MISSION.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_TRIGGER_AMBUSH.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\GF\\L_GF_CAPTAINS_CANNON.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\GF\\L_MAST_TELEPORT.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_JAIL_WALLS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_QB_ENEMY_STUNNER.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_PET_DIG_BUILD.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\GF\\L_SPAWN_LION_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_BASE_ENEMY_APE.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_GF_APE_SMASHING_QB.lua");
	CHECK_CACHE("scripts\\zone\\PROPERTY\\GF\\L_ZONE_GF_PROPERTY.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_ARCHWAY.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_MAELSTROM_GEYSER.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_PIRATE_REP.lua");
	CHECK_CACHE("scripts\\ai\\GF\\L_GF_PARROT_CRASH.lua");
	CHECK_CACHE("scripts\\ai\\MINIGAME\\SG_GF\\SERVER\\SG_CANNON.lua");
	CHECK_CACHE("scripts\\ai\\MINIGAME\\SG_GF\\L_ZONE_SG_SERVER.lua");
	CHECK_CACHE("scripts\\client\\ai\\PR\\L_PR_WHISTLE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\PR\\L_PR_SEAGULL_FLY.lua");
	CHECK_CACHE("scripts\\ai\\PETS\\L_HYDRANT_SMASHABLE.lua");
	CHECK_CACHE("scripts\\02_server\\map\\PR\\L_HYDRANT_BROKEN.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\PET_DIG_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SKELETON_DRAGON_PET_DIG_SERVER.lua");
	CHECK_CACHE("scripts\\client\\ai\\PR\\L_CRAB_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Pets\\L_PET_FROM_DIG_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Pets\\L_PET_FROM_OBJECT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Pets\\L_DAMAGING_PET.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\PR\\L_SPAWN_GRYPHON_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\L_ACT_CANDLE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\L_ENEMY_RONIN_SPAWNER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_CAVALRY.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_ACT_NINJA_TURRET_1.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\L_FV_HORSEMEN_TRIGGER.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_FLYING_CREVICE_DRAGON.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\FV\\L_FV_MAELSTROM_DRAGON.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_DRAGON_SMASHING_GOLEM_QB.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_TREASURE_CHEST_DRAGON_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\GENERAL\\L_INSTANCE_EXIT_TRANSFER_PLAYER_TO_LAST_NON_INSTANCE.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_NPC_FREE_GF_NINJAS.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_PANDA_SPAWNER_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_PANDA_SERVER.lua");
	CHECK_CACHE("scripts\\zone\\PROPERTY\\FV\\L_ZONE_FV_PROPERTY.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_BRICK_PUZZLE_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_CONSOLE_LEFT_QUICKBUILD.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_CONSOLE_RIGHT_QUICKBUILD.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_FACILITY_BRICK.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_FACILITY_PIPES.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\L_IMG_BRICK_CONSOLE_QB.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_ACT_PARADOX_PIPE_FIX.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_NINJA_GUARDS.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_ACT_PASS_THROUGH_WALL.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_ACT_BOUNCE_OVER_WALL.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\L_NPC_FONG.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_FV_MAELSTROM_GEYSER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\Racing\\RACE_SHIP_LAP_COLUMNS_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP1_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP2_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_DRAGON_LAP3_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_ABC_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_PILLAR_D_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\Racing\\RACE_FIREBALLS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_EXPLODING_ASSET.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_WISHING_WELL_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\ACT\\L_ACT_PLAYER_DEATH_TRIGGER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_GROWING_FLOWER_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_TOKEN_CONSOLE_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\ACT\\FootRace\\L_ACT_BASE_FOOT_RACE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_PROP_PLATFORM.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\VE\\L_VE_BRICKSAMPLE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_MAIL_BOX_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\ACT\\L_ACT_MINE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_WANDERING_VENDOR.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_CRATE_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\ACT\\L_ACT_VEHICLE_DEATH_TRIGGER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\RACE_IMAGINE_POWERUP.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\Racing\\RACE_MAELSTROM_GEISER.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\FV_RACE_SMASH_EGG_IMAGINE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\FV\\Racing\\FV_RACING_COLUMNS.lua");
	CHECK_CACHE("scripts\\ai\\RACING\\OBJECTS\\RACE_SMASH_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_SENTINELWALKWAY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_PARADOXTELE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_DARKITECT_REVEAL_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_BANK_INTERACT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_VENTURESPEEDPAD_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_VENTURE_CANNON_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_DUMMY.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\\\L_NT_COMBAT_EXPLODING_TARGET.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_BASE_INTERACT_DROP_LOOT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_ASSEMBLYTUBE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_PARADOX_PANEL_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_IMAG_BEAM_BUFFER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_BEAM_IMAGINATION_COLLECTORS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_DIRT_CLOUD_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_CONSOLE_TELEPORT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_SPAWN_STEGO_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_SPAWN_SABERCAT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_SPAWN_SHRAKE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_DUKE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_HAEL_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_FACTION_SPY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_OVERBUILD_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_VANDA_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_FORCE_VOLUME_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_FRICTION_VOLUME_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_XRAY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_SLEEPING_GUARD.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_IMAGIMETER_VISIBILITY_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_PIPE_VISIBILITY_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\MINIGAME\\Objects\\MINIGAME_BLUE_MARK.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_NAOMI_BREADCRUMB_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\NT\\L_NT_NAOMI_DIRT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_AM_CONSOLE_TELEPORT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_FIN.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_PIT.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_STR.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_RANDOM_SPAWNER_ZIP.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_MECH.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_BRIDGE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_DRAW_BRIDGE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SHIELD_GENERATOR_QUICKBUILD.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_DROPSHIP_COMPUTER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SCROLL_READER_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_TEMPLE_SKILL_VOLUME.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\AM\\L_AM_SKELETON_ENGINEER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SKULLKIN_DRILL_STAND.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_SKULLKIN_TOWER.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\AM\\L_AM_NAMED_DARKLING_DRAGON.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\AM\\L_AM_DARKLING_APE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_BLUE_X.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\AM\\L_TEAPOT_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_GARMADON_CELEBRATION_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_WU_NPC.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_SCROLL_CHEST_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_COLE_NPC.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_JAY_MISSION_ITEMS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_NPC_MISSION_SPINJITZU_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_ENEMY_SKELETON_SPAWNER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_NJ_RAIL_SWITCH.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_ACTIVATORS_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\Ninjago\\L_RAIL_POST_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\Ninjago\\L_ICE_RAIL_ACTIVATOR_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_FALLING_TILE.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\General\\L_ENEMY_NJ_BUFF_STUN_IMMUNITY.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_IMAGINATION_SHRINE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_LIEUTENANT.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_RAIN_OF_ARROWS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_CAVE_PRISON_CAGE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\boss_instance\\L_MONASTERY_BOSS_INSTANCE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_CATAPULT_BOUNCER_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_CATAPULT_BASE_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\Ninjago\\L_NJHUB_LAVA_PLAYER_DEATH_TRIGGER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_MON_CORE_NOOK_DOORS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_MON_CORE_SMASHABLE_DOORS.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_FLAME_JET_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_BURNING_TILE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_SPAWN_EARTH_PET_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_EARTH_PET_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_DRAGON_EMBLEM_CHEST_SERVER.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\njhub\\L_NYA_MISSION_ITEMS.lua");
	CHECK_CACHE("scripts\\02_server\\DLU\\DLUVanityTeleportingObject.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_STROMBIE.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARKLING_MECH.lua");
	CHECK_CACHE("scripts\\02_server\\Enemy\\Survival\\L_AG_SURVIVAL_DARK_SPIDERLING.lua");
	CHECK_CACHE("scripts\\EquipmentScripts\\Sunflower.lua");
	CHECK_CACHE("scripts/EquipmentScripts/AnvilOfArmor.lua");
	CHECK_CACHE("scripts/EquipmentScripts/FountainOfImagination.lua");
	CHECK_CACHE("scripts/EquipmentScripts/CauldronOfLife.lua");
	CHECK_CACHE("scripts\\02_server\\Equipment\\L_BOOTYDIG_SERVER.lua");
	CHECK_CACHE("scripts\\EquipmentScripts\\PersonalFortress.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_PROPERTY_DEVICE.lua");
	CHECK_CACHE("scripts\\02_server\\Map\\General\\L_IMAG_BACKPACK_HEALS_SERVER.lua");
	CHECK_CACHE("scripts\\ai\\GENERAL\\L_LEGO_DIE_ROLL.lua");
	CHECK_CACHE("scripts\\EquipmentScripts\\BuccaneerValiantShip.lua");
	CHECK_CACHE("scripts\\EquipmentScripts\\FireFirstSkillonStartup.lua");
	CHECK_CACHE("scripts\\equipmenttriggers\\gempack.lua");
	CHECK_CACHE("scripts\\equipmenttriggers\\shardarmor.lua");
	CHECK_CACHE("scripts\\equipmenttriggers\\coilbackpack.lua");
	CHECK_CACHE("scripts\\EquipmentScripts\\stunImmunity.lua");
	CHECK_CACHE("scripts\\ai\\NS\\WH\\L_ROCKHYDRANT_BROKEN.lua");
	CHECK_CACHE("scripts\\ai\\NS\\L_NS_WH_FANS.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\WBL_generic_zone.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_TRIGGER_GAS.lua");
	CHECK_CACHE("scripts\\ai\\FV\\L_ACT_NINJA_SENSEI.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_1_BRONZE-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_1_SILVER-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_10_BRONZE-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_10_GOLD-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_10_SILVER-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_25_BRONZE-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_25_GOLD-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_25_SILVER-COIN-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_IMAGINE-POWERUP-SPAWNER-2PT.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_LIFE-POWERUP-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_ARMOR-POWERUP-SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\SPEC\\L_SPECIAL_SPEED_BUFF_SPAWNER.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_GF_RAT.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_GF_SNAIL.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_GF_GLOWBUG.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_AMBIENT_CRAB.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_PANTS.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_NINJA_BRICKS.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_NINJA_STUDENT.lua");
	CHECK_CACHE("scripts\\ai\\WILD\\L_WILD_NINJA_SENSEI.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenBlue.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenGreen.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenOrange.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenRed.lua");
	CHECK_CACHE("scripts\\zone\\LUPs\\RobotCity Intro\\WBL_RCIntro_RobotCitizenYellow.lua");
}
