#include "GrantRoutes.h"

#include <ctime>
#include <map>

#include "AccountRules.h"
#include "Database.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "master/PlayerAction.h"
#include "PlayerActions.h"
#include "HTTPContext.h"
#include "PermissionGrants.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "SettingsRoutes.h"
#include "Web.h"
#include "WSRoutes.h"

using namespace RouteUtils;
using AccountRules::eAccountAction;
using PermissionGrants::eKind;

namespace {
	constexpr size_t MAX_NOTE = 255;
	constexpr size_t MAX_NAME = 64;
	constexpr uint32_t MAX_LIST = 500;
	constexpr uint32_t HISTORY_LENGTH = 200;
	constexpr const char* MANAGE = "grants_manage";

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	std::string Trim(std::string text) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text;
	}

	std::string UtcTime(int64_t time) {
		const auto seconds = static_cast<std::time_t>(time);
		std::tm tm{};
		gmtime_r(&seconds, &tm);
		char text[32];
		std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", &tm);
		return text;
	}

	// Who a grant is for
	struct Target {
		std::string type;     // PermissionGrants::ACCOUNT or CHARACTER
		int64_t id{};         // account ID or charinfo ID
		uint32_t accountId{}; // the account (the character's owner)
		std::string name;

		std::string Describe() const {
			return (type == PermissionGrants::ACCOUNT ? "account " : "character ") + name + " (" + std::to_string(id) + ")";
		}
		AuditTarget Audit() const {
			return type == PermissionGrants::ACCOUNT ? AuditTarget::Account(accountId) : AuditTarget{ accountId, id };
		}
	};

	std::optional<Target> AccountTarget(uint32_t accountId) {
		const auto account = Database::Get()->GetAccountById(accountId);
		if (account.contains("error")) return std::nullopt;
		return Target{ std::string(PermissionGrants::ACCOUNT), accountId, accountId, account.value("name", std::string{}) };
	}

	std::optional<Target> CharacterTarget(LWOOBJID characterId) {
		const auto info = Database::Get()->GetCharacterInfo(characterId);
		if (!info) return std::nullopt;
		return Target{ std::string(PermissionGrants::CHARACTER), characterId, info->accountId, info->name };
	}

	// {targetType, target}: an account's ID or name, or a character's ID or name
	std::optional<Target> ResolveTarget(const std::string& type, const nlohmann::json& value) {
		std::string text = value.is_string() ? Trim(value.get<std::string>()) : value.is_number_integer() ? std::to_string(value.get<int64_t>()) : "";
		if (text.empty()) return std::nullopt;
		if (type == PermissionGrants::ACCOUNT) {
			if (const auto id = GeneralUtils::TryParse<uint32_t>(text)) return AccountTarget(*id);
			const auto info = Database::Get()->GetAccountInfo(text);
			return info ? AccountTarget(info->id) : std::nullopt;
		}
		if (type == PermissionGrants::CHARACTER) {
			const auto id = ResolveCharacter(text);
			return id ? CharacterTarget(*id) : std::nullopt;
		}
		return std::nullopt;
	}

	std::optional<Target> TargetOf(const IPermissionGrants::Grant& grant) {
		if (grant.targetType == PermissionGrants::ACCOUNT) return AccountTarget(static_cast<uint32_t>(grant.targetId));
		return CharacterTarget(grant.targetId);
	}

	std::optional<PermissionGrants::Command> FindCommand(const std::vector<SlashCommandNow>& commands, const std::string& name) {
		for (const auto& command : commands) if (command.rules.name == name) return command.rules;
		return std::nullopt;
	}

	// Why the signed-in user may not give or take away this (empty: they may): only what they hold themselves
	std::string Refusal(const HTTPContext& context, eKind kind, const std::string& name, const std::vector<SlashCommandNow>& commands) {
		const auto now = Now();
		return PermissionGrants::Refusal(kind, name, context.gmLevel,
			[&context](const std::string& key) { return Can(context, key); },
			[&commands](const std::string& command) { return FindCommand(commands, command); },
			[&context, now](const PermissionGrants::Command& command) {
				return PermissionGrants::MayUseCommand(context.gmLevel, context.gmLevel, command, context.grants.get(), now);
			});
	}

	std::string Status(const IPermissionGrants::Grant& grant, int64_t now) {
		if (grant.revokedAt != 0) return "removed";
		if (grant.expiresAt != 0 && grant.expiresAt <= now) return "expired";
		return "active";
	}

	// Rows as JSON, with who they are for and whether the signed-in user may remove them
	class Lister {
	public:
		Lister(const HTTPContext& context) : context(context), commands(CurrentSlashCommands()), now(Now()) {}

		nlohmann::json Row(const IPermissionGrants::Grant& grant) {
			const auto kind = PermissionGrants::ParseKind(grant.kind);
			const auto status = Status(grant, now);
			const auto& target = Find(grant);
			bool canRemove = false;
			if (kind && status == "active" && target && Can(context, MANAGE)) {
				canRemove = MayManage(target->accountId) && Refusal(context, *kind, grant.name, commands).empty();
			}
			return {
				{"id", grant.id}, {"targetType", grant.targetType}, {"targetId", std::to_string(grant.targetId)},
				{"targetName", target ? target->name : ""}, {"accountId", target ? target->accountId : 0},
				{"kind", grant.kind}, {"name", grant.name}, {"label", kind ? PermissionGrants::Describe(*kind, grant.name) : grant.kind + " " + grant.name},
				{"deny", grant.deny}, {"expiresAt", grant.expiresAt}, {"note", grant.note}, {"grantedAt", grant.grantedAt}, {"grantedBy", grant.grantedBy},
				{"revokedAt", grant.revokedAt}, {"revokedBy", grant.revokedBy}, {"status", status}, {"canRemove", canRemove}
			};
		}

		nlohmann::json Rows(const std::vector<IPermissionGrants::Grant>& grants) {
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& grant : grants) rows.push_back(Row(grant));
			return rows;
		}

	private:
		const std::optional<Target>& Find(const IPermissionGrants::Grant& grant) {
			const auto key = grant.targetType + ":" + std::to_string(grant.targetId);
			auto it = targets.find(key);
			if (it == targets.end()) it = targets.emplace(key, TargetOf(grant)).first;
			return it->second;
		}

		bool MayManage(uint32_t accountId) {
			auto it = manageable.find(accountId);
			if (it == manageable.end()) {
				const auto account = Database::Get()->GetAccountById(accountId);
				const bool may = !account.contains("error") &&
					CanManageAccount(context, static_cast<uint8_t>(account.value("gm_level", 0)), accountId, eAccountAction::MODERATION);
				it = manageable.emplace(accountId, may).first;
			}
			return it->second;
		}

		const HTTPContext& context;
		std::vector<SlashCommandNow> commands;
		int64_t now;
		std::map<std::string, std::optional<Target>> targets;
		std::map<uint32_t, bool> manageable;
	};

	/**
	 * Grants apply at once: the dashboard's open pages get their account's rights again, and the world servers read the
	 * grants of the account's online sessions (or the character, if it is loaded) again, as for a GM level change.
	 * Returns the request ID of the refresh (the page shows its result).
	 */
	uint32_t Applied(const HTTPContext& context, const Target& target) {
		Game::web.RecheckWebSockets(target.accountId);
		BroadcastTableChanged("grants", std::to_string(target.accountId));
		PlayerActionRequest request;
		const bool account = target.type == PermissionGrants::ACCOUNT;
		request.action = account ? ePlayerAction::REFRESH_ACCOUNT : ePlayerAction::REFRESH_CHARACTER;
		request.accountId = target.accountId;
		request.characterId = account ? 0 : target.id;
		return PlayerActions::Request(request, context.accountId, [account](const PlayerActionResult& result) {
			if (result.affected == 0) return PlayerActions::Outcome{ true, account ? "The account isn't online; it applies when they next play" : "The character isn't online; it applies when they next play" };
			return PlayerActions::Outcome{ true, "Applied in game at once" };
		});
	}
}

void GrantRoutes::RegisterRoutes() {
	Route(eHTTPMethod::GET, "/api/grants/catalog", Perm(MANAGE),
		"What can be granted: the permissions, permission groups (categories), slash commands and command groups (GM levels), each with "
		"whether you may grant it ('grantable') and why not ('reason'): only what you hold yourself",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto commands = CurrentSlashCommands();
			nlohmann::json permissions = nlohmann::json::array();
			for (const auto& permission : Permissions::All()) {
				const auto reason = Refusal(context, eKind::PERMISSION, permission.key, commands);
				permissions.push_back({ {"key", permission.key}, {"title", permission.title}, {"category", permission.category},
					{"description", permission.description}, {"level", Permissions::Level(permission.key)}, {"grantable", reason.empty()}, {"reason", reason} });
			}
			nlohmann::json groups = nlohmann::json::array();
			for (const auto& group : PermissionGrants::PermissionGroups()) {
				const auto reason = Refusal(context, eKind::PERMISSION_GROUP, group, commands);
				groups.push_back({ {"name", group}, {"permissions", PermissionGrants::GroupPermissions(group)}, {"grantable", reason.empty()}, {"reason", reason} });
			}
			nlohmann::json commandList = nlohmann::json::array();
			for (const auto& command : commands) {
				if (command.clientHandled) continue;
				const auto reason = Refusal(context, eKind::COMMAND, command.rules.name, commands);
				commandList.push_back({ {"name", command.rules.name}, {"aliases", command.aliases}, {"help", command.help}, {"level", command.rules.level},
					{"minLevel", command.rules.minLevel}, {"permission", command.rules.permission}, {"grantable", reason.empty()}, {"reason", reason} });
			}
			nlohmann::json commandGroups = nlohmann::json::array();
			for (uint8_t level = 1; level <= Permissions::MAX_LEVEL; level++) {
				const auto name = std::to_string(level);
				const auto reason = Refusal(context, eKind::COMMAND_GROUP, name, commands);
				commandGroups.push_back({ {"name", name}, {"grantable", reason.empty()}, {"reason", reason} });
			}
			JsonSuccess(reply, { {"permissions", permissions}, {"permissionGroups", groups}, {"commands", commandList}, {"commandGroups", commandGroups} });
		});

	Route(eHTTPMethod::GET, "/api/grants", 0,
		"Permission grants. ?account=ID or ?character=ID: every grant of that account or character, removed and expired ones too, newest first "
		"(your own without grants_manage). Neither: {active, history} for every account and character (grants_manage)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountText = QueryValue(context.queryString, "account");
			const auto characterText = QueryValue(context.queryString, "character");
			std::optional<Target> target;
			if (!accountText.empty()) {
				const auto id = GeneralUtils::TryParse<uint32_t>(accountText);
				if (id) target = AccountTarget(*id);
			} else if (!characterText.empty()) {
				const auto id = GeneralUtils::TryParse<LWOOBJID>(characterText);
				if (id) target = CharacterTarget(*id);
			} else {
				if (!Can(context, MANAGE)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Insufficient permissions");
				Lister lister(context);
				const auto now = Now();
				return JsonSuccess(reply, { {"active", lister.Rows(Database::Get()->GetRecentPermissionGrants(true, now, MAX_LIST))},
					{"history", lister.Rows(Database::Get()->GetRecentPermissionGrants(false, now, HISTORY_LENGTH))} });
			}
			if (!target) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account or character not found");
			const bool own = context.accountId != 0 && target->accountId == context.accountId;
			if (!own && !Can(context, MANAGE)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Insufficient permissions");
			Lister lister(context);
			JsonSuccess(reply, { {"target", { {"type", target->type}, {"id", std::to_string(target->id)}, {"accountId", target->accountId}, {"name", target->name} }},
				{"grants", lister.Rows(Database::Get()->GetPermissionGrants(target->type, target->id))}, {"canManage", Can(context, MANAGE)} });
		});

	Route(eHTTPMethod::POST, "/api/grants", Perm(MANAGE),
		"Grant (or with deny: true, take away) something for one account or character. Body: {targetType: account|character, target: ID or name, "
		"kind: permission|command|permission_group|command_group, name, deny, expiresAt (Unix seconds; 0 or missing: never), note}. Only what you hold "
		"yourself, on accounts you may manage (self_moderation for your own). Online players get it at once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto target = ResolveTarget(body->value("targetType", ""), body->contains("target") ? (*body)["target"] : nlohmann::json());
			if (!target) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such account or character");
			const auto kind = PermissionGrants::ParseKind(body->value("kind", ""));
			if (!kind) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind must be permission, command, permission_group or command_group");
			const auto name = Trim(body->value("name", ""));
			if (name.empty() || name.size() > MAX_NAME) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick what to grant");
			const bool deny = body->value("deny", false);
			const auto note = Trim(body->value("note", ""));
			if (note.size() > MAX_NOTE) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The note is too long");
			const auto& expiry = body->contains("expiresAt") ? (*body)["expiresAt"] : nlohmann::json();
			if (!expiry.is_null() && !expiry.is_number_integer()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "expiresAt must be Unix seconds");
			const int64_t expiresAt = expiry.is_null() ? 0 : expiry.get<int64_t>();
			const auto now = Now();
			if (expiresAt < 0 || (expiresAt != 0 && expiresAt <= now)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The expiry must be in the future");

			// The rank rules, like every account tool: never a higher GM level, your own only with self_moderation
			if (!AuthorizeAccountAction(context, target->accountId, reply, eAccountAction::MODERATION)) return;
			const auto commands = CurrentSlashCommands();
			const auto refusal = Refusal(context, *kind, name, commands);
			if (!refusal.empty()) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, refusal);

			for (const auto& existing : Database::Get()->GetPermissionGrants(target->type, target->id)) {
				if (Status(existing, now) == "active" && existing.kind == PermissionGrants::KindName(*kind) && existing.name == name && existing.deny == deny) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "This " + target->type + " already has that; remove it first to change it");
				}
			}

			IPermissionGrants::Grant grant;
			grant.targetType = target->type;
			grant.targetId = target->id;
			grant.kind = std::string(PermissionGrants::KindName(*kind));
			grant.name = name;
			grant.deny = deny;
			grant.expiresAt = expiresAt;
			grant.note = note;
			grant.grantedAt = now;
			grant.grantedById = context.accountId;
			grant.grantedBy = context.authenticatedUser;
			grant.id = Database::Get()->InsertPermissionGrant(grant);

			const auto what = PermissionGrants::Describe(*kind, name);
			const auto description = std::string(deny ? "Took away " : "Granted ") + what + (deny ? " from " : " to ") + target->Describe() +
				(expiresAt ? " until " + UtcTime(expiresAt) : "") + (note.empty() ? "" : ": " + note) + OwnAccountNote(context.accountId, target->accountId);
			Audit(context, deny ? "deny_permission" : "grant_permission", description, target->Audit());
			const auto requestId = Applied(context, *target);
			JsonSuccess(reply, { {"id", grant.id}, {"requestId", requestId}, {"message", std::string(deny ? "Took away " : "Granted ") + what} });
		});

	Route(eHTTPMethod::POST, "/api/grants/:id/remove", Perm(MANAGE),
		"Remove a grant (or deny) that is in force: only one for something you hold yourself, on an account you may manage. Online players lose it at once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 2);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid grant ID");
			const auto grant = Database::Get()->GetPermissionGrant(*id);
			if (!grant) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Grant not found");
			if (Status(*grant, Now()) != "active") return JsonError(reply, eHTTPStatusCode::CONFLICT, "This grant isn't in force any more");
			const auto kind = PermissionGrants::ParseKind(grant->kind);
			const auto target = TargetOf(*grant);
			if (!kind || !target) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "The grant's account or character no longer exists");
			if (!AuthorizeAccountAction(context, target->accountId, reply, eAccountAction::MODERATION)) return;
			const auto refusal = Refusal(context, *kind, grant->name, CurrentSlashCommands());
			if (!refusal.empty()) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, refusal);
			if (!Database::Get()->RevokePermissionGrant(grant->id, context.authenticatedUser, Now())) {
				return JsonError(reply, eHTTPStatusCode::CONFLICT, "This grant was already removed");
			}
			const auto what = PermissionGrants::Describe(*kind, grant->name);
			Audit(context, "remove_grant", std::string("Removed the ") + (grant->deny ? "deny of " : "grant of ") + what + (grant->deny ? " from " : " to ") +
				target->Describe() + " (given by " + grant->grantedBy + ")" + OwnAccountNote(context.accountId, target->accountId), target->Audit());
			const auto requestId = Applied(context, *target);
			JsonSuccess(reply, { {"requestId", requestId}, {"message", std::string("Removed the ") + (grant->deny ? "deny of " : "grant of ") + what} });
		});
}
