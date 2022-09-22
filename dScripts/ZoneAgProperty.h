#pragma once
#include "BasePropertyServer.h"

class ZoneAgProperty : public BasePropertyServer {
public:
	void SetGameVariables(Entity* self) override;
	void OnStartup(Entity* self) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
	void OnZoneLoadedInfo(Entity* self);
	void OnZonePropertyRented(Entity* self, Entity* player) override;
	void OnZonePropertyModelPlaced(Entity* self, Entity* player) override;
	void OnZonePropertyModelPickedUp(Entity* self, Entity* player) override;
	void OnZonePropertyModelRemoved(Entity* self, Entity* player) override;
	void OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) override;
	void OnZonePropertyModelRotated(Entity* self, Entity* player) override;
	void OnZonePropertyEditBegin(Entity* self) override;
	void OnZonePropertyModelEquipped(Entity* self) override;
	void OnZonePropertyEditEnd(Entity* self) override;
	void OnPlayerExit(Entity* self);
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override {
		BaseOnFireEventServerSide(self, sender, args);
	}
	virtual void BaseOnFireEventServerSide(Entity* self, Entity* sender, std::string args);
	void OnTimerDone(Entity* self, std::string timerName) override;

	void BaseTimerDone(Entity* self, const std::string& timerName) override;

	void PropGuardCheck(Entity* self, Entity* player) override;
	void LoadInstance(Entity* self);
	void LoadProperty(Entity* self);

	void ProcessGroupObjects(Entity* self, std::string group);

	void SpawnSpots(Entity* self) override;
	void KillSpots(Entity* self) override;
	void StartMaelstrom(Entity* self, Entity* player) override;
	void SpawnCrashedRocket(Entity* self);
	void KillCrashedRocket(Entity* self);

	uint32_t RetrieveSpawnerId(Entity* self, const std::string& spawner);

	void NotifyDie(Entity* self);
	void RemovePlayerRef(Entity* self);
protected:
	std::string destroyedCinematic = "DestroyMaelstrom";
	std::vector<uint32_t> ROFTargetGroupIdTable{};
	std::u16string LandTargetGroup = u"LandTargetGroup";
	std::u16string SpiderScreamGroup = u"SpiderScreamGroup";
	std::u16string ROFTargetsGroup = u"ROFTargetsGroup";
	std::u16string SpiderEggsGroup = u"SpiderEggsGroup";
	std::u16string RocksGroup = u"RocksGroup";
	std::u16string ZoneVolumesGroup = u"ZoneVolumesGroup";

	std::u16string EnemiesSpawner = u"EnemiesSpawner";
	std::u16string BossSensorSpawner = u"BossSensorSpawner";
	std::u16string LandTargetSpawner = u"LandTargetSpawner";
	std::u16string SpiderScreamSpawner = u"SpiderScreamSpawner";
	std::u16string ROFTargetsSpawner = u"ROFTargetsSpawner";
	std::u16string SpiderEggsSpawner = u"SpiderEggsSpawner";
	std::u16string RocksSpawner = u"RocksSpawner";
	std::u16string SpiderRocketSpawner = u"SpiderRocketSpawner";
	std::u16string MailboxSpawner = u"MailboxSpawner";
	std::u16string LauncherSpawner = u"LauncherSpawner";
	std::u16string InstancerSpawner = u"InstancerSpawner";
};
