#include "Strikes.h"

#include <ctime>

#include "RouteUtils.h"
#include "GameLabels.h"
#include "StrikeSteps.h"
#include "AccountModeration.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Game.h"
#include "Web.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"

using namespace RouteUtils;

namespace {
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;

	int64_t ExpiryDays() {
		return Game::config ? GeneralUtils::TryParse<int64_t>(Game::config->GetValue("strike_expiry_days")).value_or(0) : 0;
	}

	std::string CharacterName(LWOOBJID id) {
		if (id == 0) return "";
		const auto info = Database::Get()->GetCharacterInfo(id);
		return info ? info->name : std::to_string(id);
	}

	nlohmann::json StrikeJson(const IAccountStrikes::Strike& strike, int64_t countsSince) {
		const auto source = magic_enum::enum_cast<eStrikeSource>(strike.source);
		return {
			{"id", strike.id}, {"account_id", strike.accountId}, {"character_id", std::to_string(strike.characterId)}, {"character_name", CharacterName(strike.characterId)},
			{"source", strike.source}, {"source_name", source ? GameLabels::Name(*source) : strike.source}, {"subject", strike.subject}, {"reason", strike.reason},
			{"given_by", strike.givenBy}, {"created_at", strike.createdAt}, {"revoked_at", strike.revokedAt}, {"revoked_by", strike.revokedBy},
			{"revoke_reason", strike.revokeReason}, {"counts", strike.revokedAt == 0 && strike.createdAt >= countsSince}
		};
	}

	// Staff with the moderation history see anyone's strikes; players see their own
	bool CanSeeStrikes(const HTTPContext& context, uint32_t accountId) {
		return Can(context, "accounts_notes") || (context.accountId == accountId && Can(context, "own_strikes"));
	}

	nlohmann::json AccountStrikesJson(const HTTPContext& context, uint32_t accountId) {
		const auto since = Strikes::CountsSince();
		nlohmann::json strikes = nlohmann::json::array();
		uint32_t active = 0;
		for (const auto& strike : Database::Get()->GetStrikes(accountId)) {
			auto json = StrikeJson(strike, since);
			if (json["counts"].get<bool>()) active++;
			strikes.push_back(std::move(json));
		}
		return { {"strikes", strikes}, {"active", active}, {"expiryDays", ExpiryDays()},
			{"canGive", Can(context, "strikes_give")}, {"canRevoke", Can(context, "strikes_revoke")} };
	}

	// Defaults match the Settings page (SettingsCatalog.cpp)
	int64_t Setting(const std::string& key, int64_t fallback) {
		return Game::config ? GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback) : fallback;
	}

	std::vector<StrikeSteps::Threshold> Thresholds() {
		const auto at = [](const std::string& key) { return static_cast<uint32_t>(std::clamp<int64_t>(Setting(key, 0), 0, 1000)); };
		return { { eStrikeStep::WARN, at("strike_warn_at"), 0 },
			{ eStrikeStep::MUTE, at("strike_mute_at"), std::clamp<int64_t>(Setting("strike_mute_days", 3), 1, 36500) },
			{ eStrikeStep::BAN, at("strike_ban_at"), std::clamp<int64_t>(Setting("strike_ban_days", 7), 0, 36500) } };
	}

	std::string Plural(int64_t count, const std::string& word) {
		return std::to_string(count) + " " + word + (count == 1 ? "" : "s");
	}

	/**
	 * Apply the strike threshold the account just reached, if any, as the moderator who gave the strike: the thresholds
	 * are the server's rules, so they don't need the moderator to hold the ban or mute permission, but they never act
	 * on an account the moderator couldn't manage.
	 */
	void ApplyStep(const HTTPContext& context, uint32_t accountId, uint64_t strikeId, Strikes::Given& given) {
		std::vector<std::pair<eStrikeStep, uint32_t>> applied;
		for (const auto& step : Database::Get()->GetAppliedStrikeSteps(accountId, Strikes::CountsSince())) {
			if (const auto value = magic_enum::enum_cast<eStrikeStep>(step.step)) applied.emplace_back(*value, step.count);
		}
		const auto threshold = StrikeSteps::Choose(Thresholds(), given.active, applied);
		if (!threshold) return;

		const auto account = Database::Get()->GetAccountById(accountId);
		if (account.contains("error")) return;
		const auto stepName = GameLabels::Name(threshold->step);
		if (!CanManageAccount(context, account.value("gm_level", 0), accountId, eAccountAction::MODERATION)) {
			given.step = "The account reached " + Plural(given.active, "strike") + " (" + stepName + "), but it isn't one you can manage, so nothing more was done";
			return;
		}

		const auto reason = "Reached " + Plural(given.active, "active strike");
		const auto now = static_cast<int64_t>(std::time(nullptr));
		switch (threshold->step) {
		case eStrikeStep::WARN:
			given.stepRequestId = AccountModeration::Warn(context, accountId, "Your account now has " + Plural(given.active, "strike") +
				". More strikes can lead to a mute or a ban.", true);
			given.step = reason + ": the account was warned";
			break;
		case eStrikeStep::MUTE: {
			// Never shortens a longer mute
			const auto until = std::max<uint64_t>(account.value("mute_expire", static_cast<uint64_t>(0)), static_cast<uint64_t>(now + threshold->days * DAY_SECONDS));
			AccountModeration::Mute(context, accountId, until, reason);
			given.step = reason + ": the account was muted for " + Plural(threshold->days, "day");
			break;
		}
		case eStrikeStep::BAN: {
			// Never shortens a longer ban
			const int64_t expires = account.value("ban_expires", static_cast<int64_t>(0));
			if (account.value("banned", 0) != 0 && (expires == 0 || (threshold->days > 0 && expires >= now + threshold->days * DAY_SECONDS))) {
				given.step = reason + "; the account is already banned for longer, so the ban was left as it is";
				break;
			}
			given.stepRequestId = AccountModeration::Ban(context, accountId, threshold->days, reason);
			given.step = reason + ": the account was banned " + (threshold->days > 0 ? "for " + Plural(threshold->days, "day") : std::string("permanently"));
			break;
		}
		}
		Database::Get()->SetStrikeStep(strikeId, std::string(magic_enum::enum_name(threshold->step)), threshold->at);
	}

	std::string Trimmed(std::string text, size_t max) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text.substr(0, max);
	}
}

namespace Strikes {
	int64_t CountsSince() {
		const auto days = ExpiryDays();
		return days > 0 ? static_cast<int64_t>(std::time(nullptr)) - days * DAY_SECONDS : 0;
	}

	uint32_t Active(uint32_t accountId) {
		return Database::Get()->CountActiveStrikes(accountId, CountsSince());
	}

	std::optional<bool> Requested(const HTTPContext& context, const nlohmann::json& body, HTTPReply& reply) {
		const auto strike = body.value("strike", false);
		if (strike && !Can(context, "strikes_give")) {
			JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Giving strikes needs the strikes_give permission; nothing was done");
			return std::nullopt;
		}
		return strike;
	}

	void Given::Into(nlohmann::json& response) const {
		response["strikes"] = active;
		if (!step.empty()) response["strikeStep"] = step;
		if (stepRequestId != 0) response["strikeStepRequestId"] = stepRequestId;
	}

	Given Give(const HTTPContext& context, uint32_t accountId, LWOOBJID characterId, eStrikeSource source, const std::string& subject, const std::string& reason) {
		IAccountStrikes::Strike strike;
		strike.accountId = accountId;
		strike.characterId = characterId;
		strike.source = std::string(magic_enum::enum_name(source));
		strike.subject = subject.substr(0, 128);
		strike.reason = Trimmed(reason, 500);
		strike.givenById = context.accountId;
		strike.givenBy = context.authenticatedUser;
		strike.createdAt = static_cast<int64_t>(std::time(nullptr));
		const auto strikeId = Database::Get()->InsertStrike(strike);
		Given given;
		given.active = Active(accountId);
		Audit(context, "give_strike", GameLabels::Name(source) + (subject.empty() ? "" : " (" + subject + ")") + (strike.reason.empty() ? "" : ": " + strike.reason) +
			"; " + std::to_string(given.active) + " active", characterId ? AuditTarget::Character(characterId) : AuditTarget::Account(accountId));
		if (strikeId != 0) ApplyStep(context, accountId, strikeId, given);
		BroadcastTableChanged("strikes", std::to_string(accountId));
		return given;
	}
}

void RegisterStrikeRoutes() {
	Route(eHTTPMethod::GET, "/api/accounts/:id/strikes", 0,
		"An account's strikes, newest first, with how many still count: {strikes, active, expiryDays}. Staff with accounts_notes, or the player with own_strikes",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid account");
			if (!CanSeeStrikes(context, *accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Not allowed");
			JsonSuccess(reply, AccountStrikesJson(context, *accountId));
		});

	Route(eHTTPMethod::GET, "/api/account/strikes", Perm("own_strikes"), "Your own strikes: {strikes, active, expiryDays}",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonSuccess(reply, AccountStrikesJson(context, context.accountId));
		});

	Route(eHTTPMethod::GET, "/api/characters/:id/strikes", Perm("strikes_give"),
		"How many strikes the account owning a character has, for deciding on a new one: {accountId, accountName, active, total}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			const auto account = Database::Get()->GetAccountById(info->accountId);
			JsonSuccess(reply, { {"accountId", info->accountId}, {"accountName", account.is_object() ? account.value("name", "") : ""},
				{"active", Strikes::Active(info->accountId)}, {"total", Database::Get()->GetStrikes(info->accountId).size()} });
		});

	Route(eHTTPMethod::POST, "/api/accounts/:id/strikes", Perm("strikes_give"), "Give an account a strike by hand. Body: {reason, character_id (optional)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			const auto body = ParseBody(context);
			if (!accountId || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid request");
			if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;
			const auto reason = Trimmed(body->value("reason", ""), 500);
			if (reason.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Say why the strike is given");
			LWOOBJID characterId = 0;
			if (const auto text = body->value("character_id", ""); !text.empty()) {
				const auto info = Database::Get()->GetCharacterInfo(GeneralUtils::TryParse<LWOOBJID>(text).value_or(0));
				if (!info || info->accountId != *accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That character isn't on this account");
				characterId = info->id;
			}
			const auto given = Strikes::Give(context, *accountId, characterId, eStrikeSource::MANUAL, "", reason);
			nlohmann::json response{ {"message", "Strike given; " + std::to_string(given.active) + " active"}, {"active", given.active} };
			given.Into(response);
			JsonSuccess(reply, response);
		});

	Route(eHTTPMethod::POST, "/api/strikes/:id/revoke", Perm("strikes_revoke"), "Revoke a strike: it stays on record but stops counting. Body: {reason}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 2);
			const auto body = ParseBody(context);
			const auto strike = id ? Database::Get()->GetStrike(*id) : std::nullopt;
			if (!strike) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such strike");
			if (strike->revokedAt != 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Already revoked");
			if (!AuthorizeAccountAction(context, strike->accountId, reply, eAccountAction::MODERATION)) return;
			const auto reason = body ? Trimmed(body->value("reason", ""), 500) : std::string{};
			Database::Get()->RevokeStrike(*id, context.authenticatedUser, reason, static_cast<int64_t>(std::time(nullptr)));
			Audit(context, "revoke_strike", "Strike " + std::to_string(*id) + " (" + strike->source + (strike->subject.empty() ? "" : ", " + strike->subject) + ")" +
				(reason.empty() ? "" : ": " + reason), strike->characterId ? AuditTarget::Character(strike->characterId) : AuditTarget::Account(strike->accountId));
			BroadcastTableChanged("strikes", std::to_string(strike->accountId));
			JsonSuccess(reply, { {"message", "Strike revoked"}, {"active", Strikes::Active(strike->accountId)} });
		});
}
