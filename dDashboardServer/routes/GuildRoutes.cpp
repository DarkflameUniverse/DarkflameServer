#include "GuildRoutes.h"

#include <ctime>

#include "Database.h"
#include "eGuildRank.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "GuildNameRules.h"
#include "HTTPContext.h"
#include "master/PlayerAction.h"
#include "PlayerActions.h"
#include "RouteUtils.h"
#include "Web.h"
#include "WSRoutes.h"

using namespace RouteUtils;

namespace {
	constexpr const char* MANAGE = "guilds_manage";
	constexpr uint32_t HISTORY_LENGTH = 200;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	std::string Actor(const HTTPContext& context) {
		return "[dashboard] " + context.authenticatedUser;
	}

	const char* StatusName(int32_t status) {
		return status == IGuilds::NAME_APPROVED ? "approved" : "pending";
	}

	nlohmann::json GuildJson(const IGuilds::Guild& guild) {
		return { {"id", std::to_string(guild.id)}, {"name", guild.name}, {"name_status", StatusName(guild.nameStatus)},
			{"founder_id", std::to_string(guild.founderId)}, {"created_at", guild.createdAt} };
	}

	void AddEvent(const HTTPContext& context, int64_t guildId, const std::string& kind, LWOOBJID characterId, const std::string& characterName, const std::string& detail) {
		Database::Get()->InsertGuildEvent({ 0, guildId, Now(), kind, characterId, characterName, Actor(context), detail });
	}

	// The chat server tells the guild's online members (it is the guild authority); the reply carries the request id
	nlohmann::json TellChat(int64_t guildId, uint32_t requester) {
		PlayerActionRequest request;
		request.action = ePlayerAction::GUILD_CHANGED;
		request.targetId = guildId;
		const auto id = PlayerActions::Request(request, requester, [](const PlayerActionResult& result) -> PlayerActions::Outcome {
			return { true, result.affected ? "The guild's online members were told." : "The chat server isn't running; members see the change when they next log in." };
		});
		return { {"requestId", id} };
	}

	std::optional<IGuilds::Guild> RequireGuild(const HTTPContext& context, HTTPReply& reply) {
		const auto guildId = PathId<int64_t>(context.path, 2);
		if (!guildId) {
			JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid guild ID");
			return std::nullopt;
		}
		auto guild = Database::Get()->GetGuild(*guildId);
		if (!guild) JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such guild");
		return guild;
	}

	// A name that is free: "Guild <id>", then "Guild <id>-2" and so on
	std::string PlaceholderName(int64_t guildId) {
		const std::string base = "Guild " + std::to_string(guildId);
		std::string name = base;
		for (int i = 2; Database::Get()->GetGuildByName(name); i++) name = base + "-" + std::to_string(i);
		return name;
	}

	void Changed(const std::string& guildId) {
		BroadcastTableChanged("guilds", guildId);
	}
}

void GuildRoutes::RegisterRoutes() {
	Route(eHTTPMethod::GET, "/guilds", Perm(MANAGE), "Guilds: members, history and name moderation", [](HTTPReply& reply, const HTTPContext& context) {
		RenderPage(reply, context, "guilds.jinja2", "guilds");
	});

	ReadRoute(eHTTPMethod::POST, "/api/tables/guilds", Perm(MANAGE), "Guilds (DataTables), newest first. Body adds {pending} (only names waiting for review)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto page = Database::Get()->GetGuildPage(request->start, request->length, request->search, body->value("pending", false));
			nlohmann::json data = nlohmann::json::array();
			for (const auto& summary : page.guilds) {
				auto row = GuildJson(summary.guild);
				row["member_count"] = summary.memberCount;
				row["leader_id"] = summary.leaderId ? std::to_string(summary.leaderId) : "";
				row["leader_name"] = summary.leaderName;
				data.push_back(std::move(row));
			}
			JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", page.total}, {"recordsFiltered", page.filtered}, {"data", data} });
		});

	Route(eHTTPMethod::GET, "/api/guilds/:id", Perm(MANAGE), "A guild with its members (rank 1 leader, 2 officer, 3 veteran, 4 recruit) and its history, newest first",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			auto result = GuildJson(*guild);
			if (const auto founder = Database::Get()->GetCharacterInfo(guild->founderId)) result["founder_name"] = founder->name;
			nlohmann::json members = nlohmann::json::array();
			for (const auto& member : Database::Get()->GetGuildMembers(guild->id)) {
				members.push_back({ {"id", std::to_string(member.characterId)}, {"name", member.name}, {"rank", member.rank}, {"joined_at", member.joinedAt} });
			}
			nlohmann::json events = nlohmann::json::array();
			for (const auto& event : Database::Get()->GetGuildEvents(guild->id, HISTORY_LENGTH)) {
				events.push_back({ {"id", event.id}, {"time", event.time}, {"kind", event.kind}, {"character_id", std::to_string(event.characterId)},
					{"character_name", event.characterName}, {"actor", event.actor}, {"detail", event.detail} });
			}
			result["members"] = members;
			result["events"] = events;
			JsonSuccess(reply, { {"guild", result} });
		});

	Route(eHTTPMethod::POST, "/api/guilds/:id/approve", Perm(MANAGE), "Approve a guild name that waits for review; other players see it from then on",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			Database::Get()->SetGuildName(guild->id, guild->name, IGuilds::NAME_APPROVED);
			AddEvent(context, guild->id, "name_approved", 0, "", guild->name);
			Audit(context, "approve_guild_name", "Guild " + std::to_string(guild->id) + ": " + guild->name);
			Changed(std::to_string(guild->id));
			JsonSuccess(reply, TellChat(guild->id, context.accountId));
		});

	Route(eHTTPMethod::POST, "/api/guilds/:id/reject", Perm(MANAGE), "Reject a guild name: the guild is renamed \"Guild <id>\". Body (optional): {reason}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			const auto body = ParseBody(context).value_or(nlohmann::json::object());
			const auto reason = body.value("reason", std::string{});
			const auto name = PlaceholderName(guild->id);
			Database::Get()->SetGuildName(guild->id, name, IGuilds::NAME_APPROVED);
			AddEvent(context, guild->id, "name_rejected", 0, "", guild->name + (reason.empty() ? "" : " (" + reason + ")"));
			Audit(context, "reject_guild_name", "Guild " + std::to_string(guild->id) + ": " + guild->name + " -> " + name + (reason.empty() ? "" : " (" + reason + ")"));
			Changed(std::to_string(guild->id));
			auto result = TellChat(guild->id, context.accountId);
			result["name"] = name;
			JsonSuccess(reply, result);
		});

	Route(eHTTPMethod::POST, "/api/guilds/:id/rename", Perm(MANAGE), "Rename a guild (approved). Body: {name} (3-30 of letters, digits, space ' - .; unique without regard to case)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			const auto body = ParseBody(context);
			if (!body || !body->contains("name") || !(*body)["name"].is_string()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Give the new name");
			const auto name = (*body)["name"].get<std::string>();
			if (!GuildNameRules::IsValid(GeneralUtils::UTF8ToUTF16(name))) {
				return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A guild name is 3 to 30 letters, digits, spaces, ' - and . (no space at either end or twice in a row)");
			}
			const auto taken = Database::Get()->GetGuildByName(name);
			if (taken && taken->id != guild->id) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Guild " + std::to_string(taken->id) + " has that name");
			Database::Get()->SetGuildName(guild->id, name, IGuilds::NAME_APPROVED);
			AddEvent(context, guild->id, "renamed", 0, "", guild->name + " -> " + name);
			Audit(context, "rename_guild", "Guild " + std::to_string(guild->id) + ": " + guild->name + " -> " + name);
			Changed(std::to_string(guild->id));
			JsonSuccess(reply, TellChat(guild->id, context.accountId));
		});

	Route(eHTTPMethod::POST, "/api/guilds/:id/disband", Perm(MANAGE), "Disband a guild: every member leaves and the guild is deleted (its history stays)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			const auto members = Database::Get()->GetGuildMembers(guild->id);
			Database::Get()->DeleteGuild(guild->id);
			AddEvent(context, guild->id, "disbanded", 0, "", guild->name + ", " + std::to_string(members.size()) + " member(s)");
			Audit(context, "disband_guild", "Guild " + std::to_string(guild->id) + ": " + guild->name + ", " + std::to_string(members.size()) + " member(s)");
			Changed(std::to_string(guild->id));
			JsonSuccess(reply, TellChat(guild->id, context.accountId));
		});

	Route(eHTTPMethod::POST, "/api/guilds/:id/members/:character/remove", Perm(MANAGE),
		"Remove a member. A leader's guild goes to the highest-ranked, longest-serving member; the last member's guild is deleted",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guild = RequireGuild(context, reply);
			if (!guild) return;
			const auto characterId = PathId<LWOOBJID>(context.path, 4);
			const auto member = characterId ? Database::Get()->GetGuildMember(*characterId) : std::nullopt;
			if (!member || member->guildId != guild->id) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That character isn't in this guild");
			Database::Get()->RemoveGuildMember(member->characterId);
			AddEvent(context, guild->id, "kicked", member->characterId, member->name, "");
			const auto remaining = Database::Get()->GetGuildMembers(guild->id);
			if (remaining.empty()) {
				Database::Get()->DeleteGuild(guild->id);
				AddEvent(context, guild->id, "disbanded", 0, "", "the last member was removed");
			} else if (member->rank == static_cast<uint8_t>(eGuildRank::LEADER)) {
				// Ordered by rank, then who joined first
				Database::Get()->SetGuildMemberRank(remaining.front().characterId, static_cast<uint8_t>(eGuildRank::LEADER));
				AddEvent(context, guild->id, "leader", remaining.front().characterId, remaining.front().name, "the leader was removed");
			}
			Audit(context, "remove_guild_member", "Guild " + std::to_string(guild->id) + " (" + guild->name + "): removed " + member->name, AuditTarget::Character(member->characterId));
			Changed(std::to_string(guild->id));
			JsonSuccess(reply, TellChat(guild->id, context.accountId));
		});
}
