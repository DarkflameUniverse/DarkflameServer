#include "BaseConsoleTeleportServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "CharacterComponent.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "eTerminateType.h"
#include "eStateChangeType.h"

void BaseConsoleTeleportServer::BaseOnUse(Entity* self, Entity* user) {
	auto* player = user;

	const auto& teleportLocString = self->GetVar<std::u16string>(u"teleportString");

	GameMessages::DisplayMessageBox messageBox;
	messageBox.target = player->GetObjectID();
	messageBox.bShow = true;
	messageBox.callbackClient = self->GetObjectID();
	messageBox.identifier = u"TransferBox";
	messageBox.imageID = 0;
	messageBox.text = teleportLocString;
	messageBox.userData = u"";
	messageBox.Send(player->GetSystemAddress());
}

void BaseConsoleTeleportServer::BaseOnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* player = sender;

	if (button == 1) {

		GameMessages::SetStunned stun;
		stun.target = player->GetObjectID();
		stun.StateChangeType = eStateChangeType::PUSH;
		stun.Originator = player->GetObjectID();
		stun.bCantAttack = true;
		stun.bCantEquip = true;
		stun.bCantInteract = true;
		stun.bCantJump = true;
		stun.bCantMove = true;
		stun.bCantTurn = true;
		stun.bCantUseItem = true;
		stun.Send(player->GetSystemAddress());

		const auto teleportFXID = self->GetVar<int32_t>(u"teleportEffectID");

		if (teleportFXID != 0) {
			const auto& teleportFXs = self->GetVar<std::vector<std::u16string>>(u"teleportEffectTypes");

			for (const auto& type : teleportFXs) {
				GameMessages::PlayFXEffect(player->GetObjectID(), teleportFXID, type, "FX" + GeneralUtils::UTF16ToWTF8(type)).Send(UNASSIGNED_SYSTEM_ADDRESS);
			}
		}

		const auto& teleIntroAnim = self->GetVar<std::u16string>(u"teleportAnim");
		auto animTime = 3.32999992370605f;
		if (!teleIntroAnim.empty()) {
			animTime = RenderComponent::PlayAnimation(player, teleIntroAnim);
		}

		UpdatePlayerTable(self, player, true);

		const auto playerID = player->GetObjectID();

		self->AddCallbackTimer(animTime, [playerID, self]() {
			auto* player = Game::entityManager->GetEntity(playerID);

			if (player == nullptr) {
				return;
			}

			GameMessages::SendDisplayZoneSummary(playerID, player->GetSystemAddress(), false, false, self->GetObjectID());
			});
	} else if (button == -1 || button == 0) {
		GameMessages::SendTerminateInteraction(player->GetObjectID(), eTerminateType::FROM_INTERACTION, player->GetObjectID());
	}
}

void BaseConsoleTeleportServer::UpdatePlayerTable(Entity* self, Entity* player, bool bAdd) {
	const auto iter = std::find(m_Players.begin(), m_Players.end(), player->GetObjectID());

	if (iter == m_Players.end() && bAdd) {
		m_Players.push_back(player->GetObjectID());
	} else if (iter != m_Players.end() && !bAdd) {
		m_Players.erase(iter);
	}
}

bool BaseConsoleTeleportServer::CheckPlayerTable(Entity* self, Entity* player) {
	const auto iter = std::find(m_Players.begin(), m_Players.end(), player->GetObjectID());

	return iter != m_Players.end();
}

void BaseConsoleTeleportServer::BaseOnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "summaryComplete") {
		TransferPlayer(self, sender, 0);
	}
}

void BaseConsoleTeleportServer::TransferPlayer(Entity* self, Entity* player, int32_t altMapID) {
	if (player == nullptr || !CheckPlayerTable(self, player)) {
		return;
	}

	GameMessages::SetStunned stun;
	stun.target = player->GetObjectID();
	stun.StateChangeType = eStateChangeType::POP;
	stun.Originator = player->GetObjectID();
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.Send(player->GetSystemAddress());

	GameMessages::SendTerminateInteraction(player->GetObjectID(), eTerminateType::FROM_INTERACTION, player->GetObjectID());

	const auto& teleportZone = self->GetVar<std::u16string>(u"transferZoneID");

	auto* const character = player->GetCharacter();
	if (character && self->HasVar(u"spawnPoint")) character->SetTargetScene(self->GetVarAsString(u"spawnPoint"));

	auto* characterComponent = player->GetComponent<CharacterComponent>();

	if (characterComponent) characterComponent->SendToZone(GeneralUtils::TryParse(GeneralUtils::UTF16ToWTF8(teleportZone), 0));

	UpdatePlayerTable(self, player, false);
}

void BaseConsoleTeleportServer::BaseOnTimerDone(Entity* self, const std::string& timerName) {

}
