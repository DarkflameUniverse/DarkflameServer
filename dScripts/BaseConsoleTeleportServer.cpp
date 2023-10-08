#include "BaseConsoleTeleportServer.h"
#include "GameMessages.h"
#include "Player.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "eTerminateType.h"
#include "eStateChangeType.h"

void BaseConsoleTeleportServer::BaseOnUse(Entity* self, Entity* user) {
	auto* player = user;

	const auto& teleportLocString = self->GetVar<std::u16string>(u"teleportString");

	GameMessages::SendDisplayMessageBox(player->GetObjectID(), true, self->GetObjectID(), u"TransferBox", 0, teleportLocString, u"", player->GetSystemAddress());
}

void BaseConsoleTeleportServer::BaseOnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* player = sender;

	if (button == 1) {

		GameMessages::SendSetStunned(
			player->GetObjectID(), eStateChangeType::PUSH, player->GetSystemAddress(), player->GetObjectID(),
			true, true, true, true, true, true, true
		);

		const auto teleportFXID = self->GetVar<int32_t>(u"teleportEffectID");

		if (teleportFXID != 0) {
			const auto& teleportFXs = self->GetVar<std::vector<std::u16string>>(u"teleportEffectTypes");

			for (const auto& type : teleportFXs) {
				GameMessages::SendPlayFXEffect(player->GetObjectID(), teleportFXID, type, "FX" + GeneralUtils::UTF16ToWTF8(type));
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

	GameMessages::SendSetStunned(
		player->GetObjectID(), eStateChangeType::POP, player->GetSystemAddress(), player->GetObjectID(),
		true, true, true, true, true, true, true
	);

	GameMessages::SendTerminateInteraction(player->GetObjectID(), eTerminateType::FROM_INTERACTION, player->GetObjectID());

	const auto& teleportZone = self->GetVar<std::u16string>(u"transferZoneID");

	static_cast<Player*>(player)->SendToZone(std::stoi(GeneralUtils::UTF16ToWTF8(teleportZone)));

	UpdatePlayerTable(self, player, false);
}

void BaseConsoleTeleportServer::BaseOnTimerDone(Entity* self, const std::string& timerName) {

}
