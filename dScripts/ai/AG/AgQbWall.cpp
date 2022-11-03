#include "AgQbWall.h"

void AgQbWall::OnRebuildComplete(Entity* self, Entity* player) {
	self->SetVar(u"player", player->GetObjectID());
	auto targetWallSpawners = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner"));
	if (targetWallSpawners != "") {
		auto groupObjs = EntityManager::Instance()->GetEntitiesInGroup(targetWallSpawners);
		for (auto* obj : groupObjs) {
			if (obj) {
				obj->SetVar(u"player", player->GetObjectID());
				obj->OnFireEventServerSide(self, "spawnMobs");
			}
		}
	}
}
