#ifndef DEVGMCOMMANDS_H
#define DEVGMCOMMANDS_H

namespace DEVGMCommands {
	void SetGMLevel(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ToggleNameplate(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ToggleSkipCinematics(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Kill(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Metrics(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Announce(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetAnnTitle(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetAnnMsg(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ShutdownUniverse(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetMinifig(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void TestMap(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ReportProxPhys(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SpawnPhysicsVerts(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Teleport(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ActivateSpawner(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void AddMission(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Boost(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Unboost(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Buff(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void BuffMe(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void BuffMed(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ClearFlag(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void CompleteMission(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void CreatePrivate(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void DebugUi(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Dismount(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ReloadConfig(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ForceSave(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Freecam(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void FreeMoney(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GetNavmeshHeight(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GiveUScore(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GmAddItem(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Inspect(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ListSpawns(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void LocRow(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Lookup(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void PlayAnimation(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void PlayEffect(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void PlayLvlFx(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void PlayRebuildFx(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Pos(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void RefillStats(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Reforge(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ResetMission(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Rot(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void RunMacro(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetControlScheme(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetCurrency(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetFlag(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetInventorySize(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetUiState(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Spawn(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SpawnGroup(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SpeedBoost(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void StartCelebration(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void StopEffect(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Toggle(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void TpAll(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void TriggerSpawner(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void UnlockEmote(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetLevel(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetSkillSlot(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetFaction(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void AddFaction(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GetFactions(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetRewardCode(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Crash(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void RollLoot(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void CastSkill(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void DeleteInven(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif  //!DEVGMCOMMANDS_H
