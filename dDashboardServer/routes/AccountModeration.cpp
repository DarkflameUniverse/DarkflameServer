#include "AccountModeration.h"

#include <ctime>

#include "RouteUtils.h"
#include "PlayerActions.h"
#include "master/PlayerAction.h"
#include "WSRoutes.h"
#include "Database.h"
#include "HTTPContext.h"

using namespace RouteUtils;

namespace {
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;

	std::string Sessions(uint32_t count) {
		return std::to_string(count) + (count == 1 ? " online session" : " online sessions");
	}
}

namespace AccountModeration {
	void Note(const HTTPContext& context, uint32_t accountId, const std::string& kind, const std::string& text) {
		Database::Get()->InsertAccountNote({ 0, accountId, kind, text, context.authenticatedUser, static_cast<int64_t>(std::time(nullptr)) });
		BroadcastTableChanged("account_notes", std::to_string(accountId));
	}

	uint32_t KickAccount(const HTTPContext& context, uint32_t accountId, eServerDisconnectIdentifiers reason) {
		PlayerActionRequest request;
		request.action = ePlayerAction::KICK_ACCOUNT;
		request.accountId = accountId;
		request.disconnectReason = static_cast<uint32_t>(reason);
		const auto actor = context.authenticatedUser;
		const auto actorId = context.accountId;
		return PlayerActions::Request(request, actorId, [accountId, actor, actorId](const PlayerActionResult& result) {
			if (result.affected == 0) return PlayerActions::Outcome{ true, "Account was not online" };
			Database::Get()->InsertAuditLog(actorId, actor, "disconnect_account", "Account ID " + std::to_string(accountId) + ": " + Sessions(result.affected) + OwnAccountNote(actorId, accountId), accountId, 0);
			return PlayerActions::Outcome{ true, "Disconnected " + Sessions(result.affected) };
		});
	}

	uint32_t Ban(const HTTPContext& context, uint32_t accountId, int64_t days, const std::string& reason) {
		const int64_t expires = days > 0 ? static_cast<int64_t>(std::time(nullptr)) + days * DAY_SECONDS : 0;
		Database::Get()->SetAccountBan(accountId, true, expires, reason);
		const auto length = days > 0 ? " for " + std::to_string(days) + " day" + (days == 1 ? "" : "s") : std::string(" permanently");
		Note(context, accountId, "ban", "Banned" + length + (reason.empty() ? "" : ": " + reason));
		Audit(context, "ban_account", "Account ID " + std::to_string(accountId) + length + (reason.empty() ? "" : ": " + reason), AuditTarget::Account(accountId));
		BroadcastTableChanged("accounts", std::to_string(accountId));
		return KickAccount(context, accountId, eServerDisconnectIdentifiers::FREE_TRIAL_EXPIRED);
	}

	void Mute(const HTTPContext& context, uint32_t accountId, uint64_t muteUntil, const std::string& reason) {
		Database::Get()->UpdateAccountUnmuteTime(accountId, muteUntil);
		const auto now = static_cast<uint64_t>(std::time(nullptr));
		Note(context, accountId, muteUntil > 0 ? "mute" : "unmute", (muteUntil > 0 ? "Muted for " + std::to_string((muteUntil - std::min(muteUntil, now) + 86399) / 86400) + " day(s)" : std::string("Unmuted")) +
			(reason.empty() ? "" : ": " + reason));
		Audit(context, muteUntil > 0 ? "mute_account" : "unmute_account", "Account ID " + std::to_string(accountId) +
			(muteUntil > 0 ? " until " + std::to_string(muteUntil) : ""), AuditTarget::Account(accountId));
		BroadcastTableChanged("accounts", std::to_string(accountId));
	}

	uint32_t Warn(const HTTPContext& context, uint32_t accountId, const std::string& text, bool tellPlayer) {
		Note(context, accountId, "warning", text);
		Audit(context, "warn_account", "Account ID " + std::to_string(accountId) + ": " + text, AuditTarget::Account(accountId));
		if (!tellPlayer) return 0;
		PlayerActionRequest request;
		request.action = ePlayerAction::WARN_ACCOUNT;
		request.accountId = accountId;
		request.text = text.substr(0, PlayerActionRequest::Utf8Length(text, PlayerActionRequest::MAX_TEXT));
		return PlayerActions::Request(request, context.accountId, [](const PlayerActionResult& result) {
			return result.affected > 0 ? PlayerActions::Outcome{ true, "The player saw the warning in game" }
				: PlayerActions::Outcome{ true, "The player isn't online; the warning is on their record" };
		});
	}
}
