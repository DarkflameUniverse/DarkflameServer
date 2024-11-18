#ifndef GMGREATERTHANZEROCOMMANDS_H
#define GMGREATERTHANZEROCOMMANDS_H

namespace GMGreaterThanZeroCommands {
	void Kick(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void MailItem(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Ban(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ApproveProperty(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Mute(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Fly(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void AttackImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GmImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void GmInvis(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void SetName(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Title(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ShowAll(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void FindPlayer(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Spectate(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif  //!GMGREATERTHANZEROCOMMANDS_H
