#include "CSRCommandHandler.h"
#include "eCSRCommand.h"
#include "eHTTPStatusCode.h"
#include "JSONUtils.h"
#include "magic_enum.hpp"
#include "json.hpp"

void CSRCommandHandler::HandleCSRCommand(HTTPReply& reply, const json& data) {
	auto check = JSONUtils::CheckRequiredData(data, { "command" });
	if (!check.empty()) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = check;
	} else {
		// get command from json
		std::string command_string = data["command"];
		// upper
		std::transform(command_string.begin(), command_string.end(), command_string.begin(), ::toupper);
		eCSRCommand command = magic_enum::enum_cast<eCSRCommand>(command_string).value_or(eCSRCommand::INVALID);
		
		switch (command) {
		case eCSRCommand::QUERY_SERVER_STATUS:
			QueryServerStatus(reply, data);
			break;
		case eCSRCommand::QUERY_CHARACTER_LOCATION:
			QueryCharacterLocation(reply, data);
			break;
		case eCSRCommand::QUERY_CHARACTER_ONLINE_STATUS:
			QueryCharacterOnlineStatus(reply, data);
			break;
		case eCSRCommand::INVENTORY_ADD_ITEM:
			InventoryAddItem(reply, data);
			break;
		case eCSRCommand::INVENTORY_DELETE_ITEM:
			InventoryDeleteItem(reply, data);
			break;
		case eCSRCommand::MODERATE_MUTE_ACCOUNT:
			ModerateMuteAccount(reply, data);
			break;
		case eCSRCommand::MODERATE_BAN_ACCOUNT:
			ModerateBanAccount(reply, data);
			break;
		case eCSRCommand::MODERATE_EDUCATE_CHARACTER:
			ModerateEducateCharacter(reply, data);
			break;
		case eCSRCommand::MODERATE_KICK_CHARACTER:
			ModerateKickCharacter(reply, data);
			break;
		case eCSRCommand::MODERATE_WARN_CHARACTER:
			ModerateWarnCharacter(reply, data);
			break;
		case eCSRCommand::MODERATE_RENAME_CHARACTER:
			ModerateRenameCharacter(reply, data);
			break;
		case eCSRCommand::MODERATE_DELETE_CHARACTER_FRIEND:
			ModerateDeleteCharacterFriend(reply, data);
			break;
		case eCSRCommand::MODERATE_KILL_CHARACTER:
			ModerateKillCharacter(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_HEALTH:
			UpdateCharacterHealth(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_ARMOR:
			UpdateCharacterArmor(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_IMAGINATION:
			UpdateCharacterImagination(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_MAX_HEALTH:
			UpdateCharacterMaxHealth(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_MAX_ARMOR:
			UpdateCharacterMaxArmor(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_MAX_IMAGINATION:
			UpdateCharacterMaxImagination(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_CURRENCY:
			UpdateCharacterCurrency(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_REPUTATION:
			UpdateCharacterReputation(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_LEGO_SCORE:
			UpdateCharacterLegoScore(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_EMOTES:
			UpdateCharacterEmotes(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_ADD_ACHIEVEMENT:
			UpdateCharacterAddAchievement(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_COMPLETE_ACHIEVEMENT:
			UpdateCharacterCompleteAchievement(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_REMOVE_ACHIEVEMENT:
			UpdateCharacterRemoveAchievement(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_POSITION_OFFLINE:
			UpdateCharacterPositionOffline(reply, data);
			break;
		case eCSRCommand::UPDATE_CHARACTER_INV_SLOT_AMOUNT:
			UpdateCharacterInvSlotAmount(reply, data);
			break;
		case eCSRCommand::UTILITY_SAVE_CHARACTER:
			UtilitySaveCharacter(reply, data);
			break;
		case eCSRCommand::UTILITY_SEND_MAIL:
			UtilitySendMail(reply, data);
			break;
		case eCSRCommand::UTILITY_GIVE_ITEM_TO_ALL_PLAYERS_ONLINE:
			UtilityGiveItemToAllPlayersOnline(reply, data);
			break;
		case eCSRCommand::METRICS_CONFIGURE:
			MetricsConfigure(reply, data);
			break;
		case eCSRCommand::DISABLE_ZONE:
			DisableZone(reply, data);
			break;
		case eCSRCommand::INIT_DONATION_AMOUNT:
			InitDonationAmount(reply, data);
			break;
		case eCSRCommand::KILL_SERVERS_COUNTDOWN:
			KillServersCountdown(reply, data);
			break;
		case eCSRCommand::DISABLE_FAQ:
			DisableFAQ(reply, data);
			break;
		case eCSRCommand::THROTTLEQUEUE:
			ThrottleQueue(reply, data);
			break;
		case eCSRCommand::GATEGM_ACCESS:
			GateGMAccess(reply, data);
			break;
		case eCSRCommand::RECONNECT_CRISP:
			ReconnectCrisp(reply, data);
			break;
		case eCSRCommand::MODERATE_KICK_ACCOUNT:
			ModerateKickAccount(reply, data);
			break;
		case eCSRCommand::TOGGLE_CRISP_SERVER:
			ToggleCrispServer(reply, data);
			break;
		case eCSRCommand::QUICK_DRAIN_SERVER:
			QuickDrainServer(reply, data);
			break;
		case eCSRCommand::QUICK_DRAIN_SERVER_RENEW:
			QuickDrainServerRenew(reply, data);
			break;
		case eCSRCommand::REPLICATE_CHARACTER:
			ReplicateCharacter(reply, data);
			break;
		case eCSRCommand::GET_SERVER_STATUS:
			GetServerStatus(reply);
			break;
		case eCSRCommand::RELOAD_SERVER_INIS:
			ReloadServerINIs(reply);
			break;
		case eCSRCommand::INVALID:
		default:
			reply.status = eHTTPStatusCode::BAD_REQUEST;
			reply.message = "{\"error\":\"Invalid Command\"}";
			break;
		}
	}
}

void CSRCommandHandler::QueryServerStatus(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::QueryCharacterLocation(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::QueryCharacterOnlineStatus(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::InventoryAddItem(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::InventoryDeleteItem(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateMuteAccount(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateBanAccount(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateEducateCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateKickCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateWarnCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateRenameCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateDeleteCharacterFriend(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateKillCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterHealth(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterArmor(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterImagination(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterMaxHealth(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterMaxArmor(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterMaxImagination(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterCurrency(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterReputation(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterLegoScore(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterEmotes(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterAddAchievement(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterCompleteAchievement(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterRemoveAchievement(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterPositionOffline(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UpdateCharacterInvSlotAmount(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UtilitySaveCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UtilitySendMail(HTTPReply& reply, const json& data) {
	

	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::UtilityGiveItemToAllPlayersOnline(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::MetricsConfigure(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::DisableZone(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::InitDonationAmount(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::KillServersCountdown(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::DisableFAQ(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ThrottleQueue(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::GateGMAccess(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ReconnectCrisp(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ModerateKickAccount(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ToggleCrispServer(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::QuickDrainServer(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::QuickDrainServerRenew(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ReplicateCharacter(HTTPReply& reply, const json& data) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::GetServerStatus(HTTPReply& reply) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}
void CSRCommandHandler::ReloadServerINIs(HTTPReply& reply) {
	reply.status = eHTTPStatusCode::NOT_IMPLEMENTED;
	reply.message = "{\"error\":\"Not Implemented\"}";
}