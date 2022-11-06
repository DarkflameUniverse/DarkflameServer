#pragma once
#include "CppScripts.h"

class BasePropertyServer : public CppScripts::Script {
public:

	virtual void SetGameVariables(Entity* self);
	virtual void CheckForOwner(Entity* self);
	virtual void PropGuardCheck(Entity* self, Entity* player);

	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override { BasePlayerLoaded(self, player); };
	void OnPlayerExit(Entity* self, Entity* player) override { BasePlayerExit(self, player); };
	void OnZonePropertyModelPlaced(Entity* self, Entity* player) override { BaseZonePropertyModelPlaced(self, player); }
	void OnZonePropertyRented(Entity* self, Entity* renter) override { BaseZonePropertyRented(self, renter); };
	void OnTimerDone(Entity* self, std::string timerName) override { BaseTimerDone(self, timerName); };

	virtual void BasePlayerLoaded(Entity* self, Entity* player);
	virtual void BaseZonePropertyRented(Entity* self, Entity* player) const;
	virtual void BaseZonePropertyModelPlaced(Entity* self, Entity* player) const;
	virtual void BasePlayerExit(Entity* self, Entity* player);
	virtual void BaseTimerDone(Entity* self, const std::string& timerName);

	void KillClouds(Entity* self);
	virtual void SpawnSpots(Entity* self);
	virtual void KillSpots(Entity* self);
	virtual void StartMaelstrom(Entity* self, Entity* player);
	virtual void StartTornadoFx(Entity* self) const;
	virtual void KillGuard(Entity* self);

	static void RequestDie(Entity* self, Entity* other);
	void DeactivateSpawner(const std::string& spawnerName);
	static void ResetSpawner(const std::string& spawnerName);
	static void DestroySpawner(const std::string& spawnerName);
	static void ActivateSpawner(const std::string& spawnerName);
	static void TriggerSpawner(const std::string& spawnerNamstatic);

	static LWOOBJID GetOwner();
protected:
	void HandleOrbsTimer(Entity* self);
	void HandleGeneratorTimer(Entity* self);
	void HandleQuickBuildTimer(Entity* self);

	// GUIDs
	std::string GUIDMaelstrom = "{7881e0a1-ef6d-420c-8040-f59994aa3357}";
	std::string GUIDPeaceful = "{c5725665-58d0-465f-9e11-aeb1d21842ba}";

	// Groups
	std::u16string PropertyPlaqueGroup = u"PropertyPlaqueGroup";
	std::u16string PropertyVendorGroup = u"PropertyVendorGroup";
	std::u16string PropertyBorderGroup = u"PropertyBorderGroup";
	std::u16string SpotsGroup = u"SpotsGroup";
	std::u16string MSCloudsGroup = u"MSCloudsGroup";
	std::u16string GeneratorFXGroup = u"GeneratorFXGroup";
	std::u16string GeneratorGroup = u"GeneratorGroup";
	std::u16string ImagOrbGroup = u"ImagOrbGroup";
	std::u16string FXManagerGroup = u"FXManagerGroup";
	std::u16string ClaimMarkerGroup = u"ClaimMarkerGroup";
	std::u16string GuardGroup = u"GuardGroup";
	std::u16string EnemiesGroup = u"EnemiesGroup";

	// Spawners
	std::u16string EnemiesSpawner = u"EnemiesSpawner";
	std::u16string PropObjsSpawner = u"PropObjsSpawner";
	std::u16string PropertyMGSpawner = u"PropertyMGSpawner";
	std::u16string DamageFXSpawner = u"DamageFXSpawner";
	std::u16string FXSpotsSpawner = u"FXSpotsSpawner";
	std::u16string GeneratorSpawner = u"GeneratorSpawner";
	std::u16string GeneratorFXSpawner = u"GeneratorFXSpawner";
	std::u16string FXManagerSpawner = u"FXManagerSpawner";
	std::u16string ImageOrbSpawner = u"ImageOrbSpawner";
	std::u16string AmbientFXSpawner = u"AmbientFXSpawners";
	std::u16string SmashablesSpawner = u"SmashablesSpawner";
	std::u16string ClaimMarkerSpawner = u"ClaimMarkerSpawner";
	std::u16string BehaviorObjsSpawner = u"BehaviorObjsSpawner";

	//Flags / constants
	std::u16string guardFirstMissionFlag = u"guardFirstMissionFlag";
	std::u16string guardMissionFlag = u"guardMissionFlag";
	std::u16string brickLinkMissionIDFlag = u"brickLinkMissionIDFlag";
	std::u16string placedModelFlag = u"placedModelFlag";
	std::u16string generatorIdFlag = u"generatorIdFlag";
	std::u16string defeatedProperyFlag = u"defeatedProperyFlag";
	std::u16string passwordFlag = u"passwordFlag";
	std::u16string orbIDFlag = u"orbIDFlag";
	std::u16string behaviorQBID = u"behaviorQBID";

	// Variables
	std::u16string PlayerIDVariable = u"playerID";
	std::u16string CollidedVariable = u"collided";
	std::u16string PropertyOwnerVariable = u"PropertyOwner";
	std::u16string PropertyOwnerIDVariable = u"PropertyOwnerID";
	std::u16string FXObjectsGoneVariable = u"FXObjectGone";
	std::u16string RenterVariable = u"renter";
	std::u16string UnclaimedVariable = u"unclaimed";

	// Events
	std::string CheckForPropertyOwnerEvent = "CheckForPropertyOwner";

	// Timers
	std::string StartGeneratorTimer = "startGenerator";
	std::string StartOrbTimer = "startOrb";
	std::string StartQuickbuildTimer = "startQuickbuild";
	std::string TornadoOffTimer = "tornadoOff";
	std::string KillMarkerTimer = "killMarker";
	std::string KillGuardTimer = "KillGuard";
	std::string ShowClearEffectsTimer = "ShowClearEffects";
	std::string TurnSkyOffTimer = "turnSkyOff";
	std::string KillStrombiesTimer = "killStrombies";
	std::string KillFXObjectTimer = "killFXObject";
	std::string ShowVendorTimer = "ShowVendor";
	std::string BoundsVisOnTimer = "BoundsVisOn";
	std::string RunPlayerLoadedAgainTimer = "runPlayerLoadedAgain";
	std::string PollTornadoFXTimer = "pollTornadoFX";
};
