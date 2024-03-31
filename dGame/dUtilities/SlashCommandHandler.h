/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SLASHCOMMANDHANDLER_H
#define SLASHCOMMANDHANDLER_H

#include "RakNetTypes.h"
#include "eGameMasterLevel.h"
#include <string>

class Entity;

struct Command {
	Command(std::string help, std::string info, std::vector<std::string> aliases, std::function<void(Entity*,const std::string)> handle, eGameMasterLevel requiredLevel = eGameMasterLevel::DEVELOPER) {
		this->help = help;
		this->info = info;
		this->aliases = aliases;
		this->handle = handle;
		this->requiredLevel = requiredLevel;
	}
	std::string help;
	std::string info;
	std::vector<std::string> aliases;
	std::function<void(Entity*,const std::string)> handle;
	eGameMasterLevel requiredLevel = eGameMasterLevel::DEVELOPER;
};

namespace SlashCommandHandler {
	void Startup();
	void HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr);
	void SendAnnouncement(const std::string& title, const std::string& message);
	void RegisterCommand(Command info);
};

namespace DEVGMCommands {
	void SetGMLevel(Entity* entity, const std::string args);
	void ToggleNameplate(Entity* entity, const std::string args);
	void ToggleSkipCinematics(Entity* entity, const std::string args);
	void Kill(Entity* entity, const std::string args);
	void Metrics(Entity* entity, const std::string args);
	void Announce(Entity* entity, const std::string args);
	void SetAnnouncementTitle(Entity* entity, const std::string args);
	void SetAnnouncementMessage(Entity* entity, const std::string args);
	void ShutdownUniverse(Entity* entity, const std::string args);
	void SetMinifig(Entity* entity, const std::string args);
	void TestMap(Entity* entity, const std::string args);
	void ReportProximityPhysics(Entity* entity, const std::string args);
	void SpawnPhysicsVerts(Entity* entity, const std::string args);
	void Teleport(Entity* entity, const std::string args);
	void ActivateSpawner(Entity* entity, const std::string args);
	void AddMission(Entity* entity, const std::string args);
	void Boost(Entity* entity, const std::string args);
	void Unboost(Entity* entity, const std::string args);
	void Buff(Entity* entity, const std::string args);
	void BuffMe(Entity* entity, const std::string args);
	void BuffMedium(Entity* entity, const std::string args);
	void ClearFlag(Entity* entity, const std::string args);
	void CompleteMission(Entity* entity, const std::string args);
	void CreatePrivateInstance(Entity* entity, const std::string args);
	void DebugUI(Entity* entity, const std::string args);
	void Dismount(Entity* entity, const std::string args);
	void ReloadConfig(Entity* entity, const std::string args);
	void ForceSave(Entity* entity, const std::string args);
	void Freecam(Entity* entity, const std::string args);
	void FreeMoney(Entity* entity, const std::string args);
	void GetNavmeshHeight(Entity* entity, const std::string args);
	void GiveUScore(Entity* entity, const std::string args);
	void GMAddItem(Entity* entity, const std::string args);
	void Inspect(Entity* entity, const std::string args);
	void ListSpawns(Entity* entity, const std::string args);
	void LocationRotation(Entity* entity, const std::string args);
	void Lookup(Entity* entity, const std::string args);
	void PlayAnimation(Entity* entity, const std::string args);
	void PlayEffect(Entity* entity, const std::string args);
	void PlayLevelFX(Entity* entity, const std::string args);
	void PlayRebuildFX(Entity* entity, const std::string args);
	void Position(Entity* entity, const std::string args);
	void RefilStats(Entity* entity, const std::string args);
	void Reforge(Entity* entity, const std::string args);
	void ResetMission(Entity* entity, const std::string args);
	void Rotation(Entity* entity, const std::string args);
	void RunMacro(Entity* entity, const std::string args);
	void SetControlScheme(Entity* entity, const std::string args);
	void SetCurrency(Entity* entity, const std::string args);
	void SetFlag(Entity* entity, const std::string args);
	void SetInventorySize(Entity* entity, const std::string args);
	void SetUIState(Entity* entity, const std::string args);
	void Spawn(Entity* entity, const std::string args);
	void SpawnGroup(Entity* entity, const std::string args);
	void SpeedBoost(Entity* entity, const std::string args);
	void StartCelebration(Entity* entity, const std::string args);
	void StopEffect(Entity* entity, const std::string args);
	void Toggle(Entity* entity, const std::string args);
	void TeleportAll(Entity* entity, const std::string args);
	void TriggerSpawner(Entity* entity, const std::string args);
	void UnlockEmote(Entity* entity, const std::string args);
	void SetLevel(Entity* entity, const std::string args);
	void SetSkillSlot(Entity* entity, const std::string args);
	void SetFaction(Entity* entity, const std::string args);
	void AddFaction(Entity* entity, const std::string args);
	void GetFactions(Entity* entity, const std::string args);
	void SetRewardCode(Entity* entity, const std::string args);

	void Crash(Entity* entity, const std::string args);
	void RollLoot(Entity* entity, const std::string args);
	void CastSkill(Entity* entity, const std::string args);
}

namespace GMZeroCommands {
	void Help(Entity* entity, const std::string args);
	void Credits(Entity* entity, const std::string args);
	void Info(Entity* entity, const std::string args);
	void Die(Entity* entity, const std::string args);
	void Ping(Entity* entity, const std::string args);
	void PVP(Entity* entity, const std::string args);
	void RequestMailCount(Entity* entity, const std::string args);
	void Who(Entity* entity, const std::string args);
	void FixStats(Entity* entity, const std::string args);
	void JoinPrivateInstance(Entity* entity, const std::string args);
	void LeaveZone(Entity* entity, const std::string args);
}

namespace GreaterThanZeroCommands {
	void Kick(Entity* entity, const std::string args);
	void MailItem(Entity* entity, const std::string args);
	void Ban(Entity* entity, const std::string args);
	void ApproveProperty(Entity* entity, const std::string args);
	void Mute(Entity* entity, const std::string args);
	void Fly(Entity* entity, const std::string args);
	void AttackImmune(Entity* entity, const std::string args);
	void GMImmune(Entity* entity, const std::string args);
	void GMInvis(Entity* entity, const std::string args);
	void SetName(Entity* entity, const std::string args);
	void Title(Entity* entity, const std::string args);
}

#endif // SLASHCOMMANDHANDLER_H
