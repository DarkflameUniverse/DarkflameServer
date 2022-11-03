#include "AgSurvivalBuffStation.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "TeamManager.h"

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
	auto destroyableComponent = self->GetComponent<DestroyableComponent>();
	// We set the faction to 1 so that the buff station sees players as friendly targets to buff
	if (destroyableComponent != nullptr) destroyableComponent->SetFaction(1);

	auto skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent != nullptr) skillComponent->CalculateBehavior(skillIdForBuffStation, behaviorIdForBuffStation, self->GetObjectID());

	self->AddCallbackTimer(smashTimer, [self]() {
		self->Smash();
		});
	self->AddTimer("DropArmor", dropArmorTimer);
	self->AddTimer("DropLife", dropLifeTimer);
	self->AddTimer("Dropimagination", dropImaginationTimer);
	// Since all survival players should be on the same team, we get the team.
	auto team = TeamManager::Instance()->GetTeam(target->GetObjectID());

	std::vector<LWOOBJID> builderTeam;
	// Not on a team
	if (team == nullptr) {
		builderTeam.push_back(target->GetObjectID());
		self->SetVar<std::vector<LWOOBJID>>(u"BuilderTeam", builderTeam);
		return;
	}

	for (auto memberID : team->members) {
		builderTeam.push_back(memberID);
	}
	self->SetVar<std::vector<LWOOBJID>>(u"BuilderTeam", builderTeam);
}

void AgSurvivalBuffStation::OnTimerDone(Entity* self, std::string timerName) {
	uint32_t powerupToDrop = lifePowerup;
	if (timerName == "DropArmor") {
		powerupToDrop = armorPowerup;
		self->AddTimer("DropArmor", dropArmorTimer);
	}
	if (timerName == "DropLife") {
		powerupToDrop = lifePowerup;
		self->AddTimer("DropLife", dropLifeTimer);
	}
	if (timerName == "Dropimagination") {
		powerupToDrop = imaginationPowerup;
		self->AddTimer("Dropimagination", dropImaginationTimer);
	}
	auto team = self->GetVar<std::vector<LWOOBJID>>(u"BuilderTeam");
	for (auto memberID : team) {
		auto member = EntityManager::Instance()->GetEntity(memberID);
		if (member != nullptr && !member->GetIsDead()) {
			GameMessages::SendDropClientLoot(member, self->GetObjectID(), powerupToDrop, 0, self->GetPosition());
		} else {
			// If player left the team or left early erase them from the team variable.
			team.erase(std::find(team.begin(), team.end(), memberID));
			self->SetVar<std::vector<LWOOBJID>>(u"BuilderTeam", team);
		}
	}
}
