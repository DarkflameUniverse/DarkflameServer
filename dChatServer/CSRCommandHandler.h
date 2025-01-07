#ifndef __CSR_COMMAND_HANDLER__H__
#define __CSR_COMMAND_HANDLER__H__

#include <optional>
#include "json_fwd.hpp"

using json = nlohmann::json;
struct HTTPReply;


namespace CSRCommandHandler {
	void HandleCSRCommand(HTTPReply& reply, const json& data);

	void QueryServerStatus(HTTPReply& reply, const json& data);
	void QueryCharacterLocation(HTTPReply& reply, const json& data);
	void QueryCharacterOnlineStatus(HTTPReply& reply, const json& data);
	void InventoryAddItem(HTTPReply& reply, const json& data);
	void InventoryDeleteItem(HTTPReply& reply, const json& data);
	void ModerateMuteAccount(HTTPReply& reply, const json& data);
	void ModerateBanAccount(HTTPReply& reply, const json& data);
	void ModerateEducateCharacter(HTTPReply& reply, const json& data);
	void ModerateKickCharacter(HTTPReply& reply, const json& data);
	void ModerateWarnCharacter(HTTPReply& reply, const json& data);
	void ModerateRenameCharacter(HTTPReply& reply, const json& data);
	void ModerateDeleteCharacterFriend(HTTPReply& reply, const json& data);
	void ModerateKillCharacter(HTTPReply& reply, const json& data);
	void UpdateCharacterHealth(HTTPReply& reply, const json& data);
	void UpdateCharacterArmor(HTTPReply& reply, const json& data);
	void UpdateCharacterImagination(HTTPReply& reply, const json& data);
	void UpdateCharacterMaxHealth(HTTPReply& reply, const json& data);
	void UpdateCharacterMaxArmor(HTTPReply& reply, const json& data);
	void UpdateCharacterMaxImagination(HTTPReply& reply, const json& data);
	void UpdateCharacterCurrency(HTTPReply& reply, const json& data);
	void UpdateCharacterReputation(HTTPReply& reply, const json& data);
	void UpdateCharacterLegoScore(HTTPReply& reply, const json& data);
	void UpdateCharacterEmotes(HTTPReply& reply, const json& data);
	void UpdateCharacterAddAchievement(HTTPReply& reply, const json& data);
	void UpdateCharacterCompleteAchievement(HTTPReply& reply, const json& data);
	void UpdateCharacterRemoveAchievement(HTTPReply& reply, const json& data);
	void UpdateCharacterPositionOffline(HTTPReply& reply, const json& data);
	void UpdateCharacterInvSlotAmount(HTTPReply& reply, const json& data);
	void UtilitySaveCharacter(HTTPReply& reply, const json& data);
	void UtilitySendMail(HTTPReply& reply, const json& data);
	void UtilityGiveItemToAllPlayersOnline(HTTPReply& reply, const json& data);
	void MetricsConfigure(HTTPReply& reply, const json& data);
	void DisableZone(HTTPReply& reply, const json& data);
	void InitDonationAmount(HTTPReply& reply, const json& data);
	void KillServersCountdown(HTTPReply& reply, const json& data);
	void DisableFAQ(HTTPReply& reply, const json& data);
	void ThrottleQueue(HTTPReply& reply, const json& data);
	void GateGMAccess(HTTPReply& reply, const json& data);
	void ReconnectCrisp(HTTPReply& reply, const json& data);
	void ModerateKickAccount(HTTPReply& reply, const json& data);
	void ToggleCrispServer(HTTPReply& reply, const json& data);
	void QuickDrainServer(HTTPReply& reply, const json& data);
	void QuickDrainServerRenew(HTTPReply& reply, const json& data);
	void ReplicateCharacter(HTTPReply& reply, const json& data);
	void GetServerStatus(HTTPReply& reply);
	void ReloadServerINIs(HTTPReply& reply);
};

#endif // !__CSR_COMMAND_HANDLER__H__