#ifndef GMZEROCOMMANDS_H
#define GMZEROCOMMANDS_H

namespace GMZeroCommands {
	void Help(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Credits(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Info(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Die(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Ping(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Pvp(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void RequestMailCount(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Who(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void FixStats(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Join(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void LeaveZone(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Resurrect(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void InstanceInfo(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void ClientHandled(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif  //!GMZEROCOMMANDS_H
