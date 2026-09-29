#include "ChatRoutes.h"

#include <ctime>
#include <map>
#include <regex>

#include "ChatHistory.h"
#include "RouteUtils.h"
#include "DashboardRoutes.h"
#include "PlayerActions.h"
#include "master/PlayerAction.h"
#include "WSRoutes.h"
#include "Permissions.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "Web.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr uint32_t MAX_PAGE = 500;
	uint64_t g_LastPushed = 0;

	ChatHistory::Access AccessOf(const HTTPContext& context) {
		return { Can(context, "chat_private"), Can(context, "chat_dms") };
	}

	nlohmann::json RowJson(const IChatLog::ChatMessage& m, const ChatHistory::Access& access) {
		const auto& zones = ZoneNames();
		const auto zone = std::to_string(m.zoneId);
		auto json = ChatHistory::MessageJson(m, access);
		json["zone_name"] = zones.contains(zone) ? zones[zone] : nlohmann::json("");
		return json;
	}

	// The messages as JSON, each with the newest flag covering it (flag_id, 0 for none)
	nlohmann::json MessagesJson(const std::vector<IChatLog::ChatMessage>& messages, const ChatHistory::Access& access) {
		std::vector<uint64_t> ids;
		for (const auto& m : messages) ids.push_back(m.id);
		std::map<uint64_t, uint64_t> flags;
		for (const auto& [message, flag] : Database::Get()->GetFlaggedMessages(ids)) flags[message] = flag;
		nlohmann::json out = nlohmann::json::array();
		for (const auto& m : messages) {
			auto json = RowJson(m, access);
			json["flag_id"] = flags.contains(m.id) ? flags[m.id] : 0;
			out.push_back(std::move(json));
		}
		return out;
	}

	// The filters both the API and the Chat Log page take, from a query string or a JSON body
	IChatLog::ChatQuery Query(const HTTPContext& context, const std::function<std::string(const char*)>& get) {
		IChatLog::ChatQuery q;
		q.channel = get("channel");
		// Team and guild chat, and whispers, need their own permissions
		const auto access = AccessOf(context);
		q.includePrivate = access.group;
		q.includeWhispers = access.whispers;
		if (!q.channel.empty() && !ChatHistory::CanRead(q.channel, access)) q.channel = "none";
		const auto character = get("character");
		if (!character.empty()) q.characterId = ResolveCharacter(character).value_or(-1);
		q.accountId = GeneralUtils::TryParse<uint32_t>(get("account")).value_or(0);
		q.zoneId = GeneralUtils::TryParse<uint32_t>(get("zone")).value_or(0);
		q.instanceId = GeneralUtils::TryParse<int64_t>(get("instance")).value_or(-1);
		q.since = std::max<int64_t>(0, GeneralUtils::TryParse<int64_t>(get("since")).value_or(0));
		q.until = std::max<int64_t>(0, GeneralUtils::TryParse<int64_t>(get("until")).value_or(0));
		q.search = get("search");
		q.blockedOnly = get("blocked") == "1" || get("blocked") == "true";
		return q;
	}

	uint32_t Limit(const HTTPContext& context, uint32_t fallback) {
		return std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(fallback), 1, MAX_PAGE);
	}

	// A page of one conversation, newest page first, going back with `before`: {messages (oldest first), has_more}
	nlohmann::json ConversationPage(IChatLog::ChatQuery q, const HTTPContext& context, const ChatHistory::Access& access) {
		q.beforeId = GeneralUtils::TryParse<uint64_t>(QueryValue(context.queryString, "before")).value_or(0);
		q.limit = Limit(context, 100) + 1; // one more, to know whether there is an older page
		q.newestFirst = true;
		auto messages = Database::Get()->GetChatMessages(q);
		const bool more = messages.size() == q.limit;
		if (more) messages.pop_back();
		std::reverse(messages.begin(), messages.end());
		return { {"messages", MessagesJson(messages, access)}, {"has_more", more} };
	}

	std::optional<std::string> CharacterName(LWOOBJID id) {
		const auto info = Database::Get()->GetCharacterInfo(id);
		if (!info) return std::nullopt;
		return info->name;
	}
}

namespace ChatRoutes {
	void PushNew(uint64_t newestId) {
		if (g_LastPushed == 0 || newestId <= g_LastPushed) { g_LastPushed = std::max(g_LastPushed, newestId); return; }
		IChatLog::ChatQuery q;
		q.afterId = g_LastPushed;
		q.limit = MAX_PAGE;
		// Only what everyone with chat_view may read goes on the topic; private chat stays behind its permission.
		// Page through everything new, so a burst of more than a page isn't skipped (bounded, in case of a flood).
		constexpr int MAX_PAGES = 20;
		for (int page = 0; page < MAX_PAGES; page++) {
			const auto messages = Database::Get()->GetChatMessages(q);
			for (const auto& message : messages) {
				auto json = RowJson(message, {});
				Game::web.SendWSMessage("chat_message", json);
			}
			if (messages.size() < q.limit) break;
			q.afterId = messages.back().id;
			if (page == MAX_PAGES - 1) LOG("More than %d chat messages arrived at once; the rest are on the Chat Log page", MAX_PAGES * static_cast<int>(MAX_PAGE));
		}
		g_LastPushed = newestId;
	}
}

void RegisterChatRoutes() {
	Game::web.RegisterWSSubscription("chat_message", std::function<uint8_t()>([] { return Permissions::Level("chat_view"); }), "chat_view");

	Route(eHTTPMethod::GET, "/api/chat", Perm("chat_view"),
		"Chat messages, oldest first. Query: after (the last id you have; for bridges polling), limit (max 500), channel (zone, whisper, team, guild, web), "
		"character (name or ID), account, zone, instance (one world server of that zone), since and until (unix times), search, blocked=1 (only what the chat filter stopped). "
		"Team and guild chat need chat_private, whispers chat_dms. Returns {messages, last_id}. Live: subscribe to the chat_message WebSocket topic",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto access = AccessOf(context);
			auto q = Query(context, [&](const char* name) { return QueryValue(context.queryString, name); });
			q.afterId = GeneralUtils::TryParse<uint64_t>(QueryValue(context.queryString, "after")).value_or(0);
			q.limit = Limit(context, 100);
			// Without `after`, the newest page (still returned oldest first)
			q.newestFirst = q.afterId == 0;
			auto messages = Database::Get()->GetChatMessages(q);
			if (q.newestFirst) std::reverse(messages.begin(), messages.end());
			nlohmann::json out = nlohmann::json::array();
			uint64_t last = q.afterId;
			for (const auto& m : messages) { out.push_back(RowJson(m, access)); last = std::max(last, m.id); }
			JsonSuccess(reply, { {"messages", out}, {"last_id", last} });
		});

	Route(eHTTPMethod::POST, "/api/tables/chat_log", Perm("chat_view"),
		"The chat log for the Chat Log page (DataTables), newest first. Body adds {channel, character, account, zone, instance, since, until, blocked}. "
		"Each row has flag_id: the newest chat flag covering it (0: none)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto access = AccessOf(context);
			auto q = Query(context, [&](const char* name) {
				// DataTables' own search is an object ({value, regex}); it's read from the request below
				if (!body->contains(name) || !(*body)[name].is_primitive() || (*body)[name].is_null()) return std::string{};
				const auto& v = (*body)[name];
				return v.is_string() ? v.get<std::string>() : v.is_boolean() ? std::string(v.get<bool>() ? "1" : "0") : v.dump();
			});
			if (q.search.empty()) q.search = request->search;
			q.newestFirst = true;
			q.offset = request->start;
			q.limit = std::clamp<uint32_t>(request->length, 1, MAX_PAGE);
			IChatLog::ChatQuery all;
			all.includePrivate = q.includePrivate;
			all.includeWhispers = q.includeWhispers;
			JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", Database::Get()->CountChatMessages(all)},
				{"recordsFiltered", Database::Get()->CountChatMessages(q)}, {"data", MessagesJson(Database::Get()->GetChatMessages(q), access)} });
		});

	Route(eHTTPMethod::GET, "/api/chat/stats", Perm("chat_view"),
		"How much is said: messages in the last hour and today, and how many the filter stopped today. Query: zone, instance (one world server)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			const auto count = [&](int64_t since, bool blocked) {
				auto q = Query(context, [&](const char* name) { return QueryValue(context.queryString, name); });
				q.since = since;
				q.blockedOnly = blocked;
				return Database::Get()->CountChatMessages(q);
			};
			JsonSuccess(reply, { {"lastHour", count(now - 3600, false)}, {"today", count(now - 86400, false)}, {"blockedToday", count(now - 86400, true)} });
		});

	Route(eHTTPMethod::GET, "/api/chat/messages/:id/context", Perm("chat_view"),
		"A message and the conversation around it: the same world for zone chat, the same two characters for whispers, the same team or guild. "
		"Query: count (messages before and after, default 10, max 50). Needs the permission for the message's channel",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 3);
			if (!id || *id == 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid message ID");
			const auto access = AccessOf(context);
			IChatLog::ChatQuery find;
			find.includePrivate = true;
			find.includeWhispers = true;
			find.afterId = *id - 1;
			find.limit = 1;
			const auto found = Database::Get()->GetChatMessages(find);
			if (found.empty() || found.front().id != *id) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such message (chat is kept for log_chat_days)");
			const auto& message = found.front();
			if (!ChatHistory::CanRead(message.channel, access)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Reading " + message.channel + " chat needs another permission");
			const auto count = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "count")).value_or(10), 1, 50);
			auto before = ChatHistory::ConversationQuery(message, access);
			before.beforeId = message.id;
			before.newestFirst = true;
			before.limit = count;
			auto older = Database::Get()->GetChatMessages(before);
			std::reverse(older.begin(), older.end());
			auto after = ChatHistory::ConversationQuery(message, access);
			after.afterId = message.id;
			after.limit = count;
			std::vector<IChatLog::ChatMessage> all = older;
			all.push_back(message);
			for (const auto& m : Database::Get()->GetChatMessages(after)) all.push_back(m);
			if (message.channel == "whisper") {
				Audit(context, "chat_dms_view", "Whispers between " + message.senderName + " and " + message.recipientName + " around message " + std::to_string(message.id),
					AuditTarget::Character(message.senderId));
			}
			JsonSuccess(reply, { {"message", RowJson(message, access)}, {"messages", MessagesJson(all, access)} });
		});

	// ---- Conversations: guild chat, team chat, whispers ----

	Route(eHTTPMethod::GET, "/api/chat/guild/:id", Perm("chat_private"),
		"A guild's chat, newest page first. Query: before (the oldest id you have, for the page before it), limit (max 500). Returns {guild, messages (oldest first), has_more}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto guildId = PathId<int64_t>(context.path, 3);
			if (!guildId || *guildId <= 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid guild ID");
			const auto access = AccessOf(context);
			IChatLog::ChatQuery q;
			q.includePrivate = true;
			q.channel = "guild";
			q.guildId = *guildId;
			auto page = ConversationPage(q, context, access);
			const auto guild = Database::Get()->GetGuild(*guildId);
			page["guild"] = { {"id", std::to_string(*guildId)}, {"name", guild ? guild->name : ""}, {"exists", guild.has_value()} };
			JsonSuccess(reply, page);
		});

	Route(eHTTPMethod::GET, "/api/chat/teams", Perm("chat_private"),
		"Teams that talked in team chat, most recent first. Query: character (name or ID: only teams they talked in), offset, limit (max 500). "
		"Returns {teams: [{team_id, messages, first_time, last_time, senders}], total}",
		[](HTTPReply& reply, const HTTPContext& context) {
			LWOOBJID character = 0;
			const auto text = QueryValue(context.queryString, "character");
			if (!text.empty()) {
				const auto resolved = ResolveCharacter(text);
				if (!resolved) return JsonSuccess(reply, { {"teams", nlohmann::json::array()}, {"total", 0} });
				character = *resolved;
			}
			const auto offset = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "offset")).value_or(0);
			nlohmann::json teams = nlohmann::json::array();
			for (const auto& team : Database::Get()->GetChatTeams(character, offset, Limit(context, 50))) {
				teams.push_back({ {"team_id", std::to_string(team.teamId)}, {"messages", team.messages}, {"first_time", team.firstTime}, {"last_time", team.lastTime},
					{"senders", GeneralUtils::SplitString(team.senders, ',')} });
			}
			JsonSuccess(reply, { {"teams", teams}, {"total", Database::Get()->CountChatTeams(character)} });
		});

	Route(eHTTPMethod::GET, "/api/chat/team/:id", Perm("chat_private"),
		"One team's chat, newest page first. Query: before, limit (max 500). Returns {messages (oldest first), has_more}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto teamId = PathId<LWOOBJID>(context.path, 3);
			if (!teamId || *teamId <= 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid team ID");
			IChatLog::ChatQuery q;
			q.includePrivate = true;
			q.channel = "team";
			q.teamId = *teamId;
			JsonSuccess(reply, ConversationPage(q, context, AccessOf(context)));
		});

	Route(eHTTPMethod::GET, "/api/characters/:id/whispers", Perm("chat_dms"),
		"The characters a character whispered with, most recent conversation first. Query: offset, limit (max 500). "
		"Returns {character, partners: [{character_id, name, messages, first_time, last_time}], total}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto characterId = PathId<LWOOBJID>(context.path, 2);
			if (!characterId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid character ID");
			const auto offset = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "offset")).value_or(0);
			nlohmann::json partners = nlohmann::json::array();
			for (const auto& p : Database::Get()->GetWhisperPartners(*characterId, offset, Limit(context, 50))) {
				partners.push_back({ {"character_id", std::to_string(p.characterId)}, {"name", p.name}, {"messages", p.messages}, {"first_time", p.firstTime}, {"last_time", p.lastTime} });
			}
			JsonSuccess(reply, { {"character", { {"id", std::to_string(*characterId)}, {"name", CharacterName(*characterId).value_or("")} }},
				{"partners", partners}, {"total", Database::Get()->CountWhisperPartners(*characterId)} });
		});

	Route(eHTTPMethod::GET, "/api/characters/:id/whispers/:other", Perm("chat_dms"),
		"The whispers between two characters, newest page first. Query: before, limit (max 500). Returns {messages (oldest first), has_more}. "
		"Opening a conversation (the first page) is audited",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto characterId = PathId<LWOOBJID>(context.path, 2);
			const auto otherId = PathId<LWOOBJID>(context.path, 4);
			if (!characterId || !otherId || *characterId == *otherId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid character IDs");
			IChatLog::ChatQuery q;
			q.includeWhispers = true;
			q.channel = "whisper";
			q.characterId = *characterId;
			q.otherCharacterId = *otherId;
			auto page = ConversationPage(q, context, AccessOf(context));
			if (QueryValue(context.queryString, "before").empty()) {
				Audit(context, "chat_dms_view", "Whispers between " + CharacterName(*characterId).value_or(std::to_string(*characterId)) + " and " +
					CharacterName(*otherId).value_or(std::to_string(*otherId)), AuditTarget::Character(*characterId));
			}
			JsonSuccess(reply, page);
		});

	Route(eHTTPMethod::GET, "/chat_log/guild/:id", Perm("chat_private"), "One guild's chat", [](HTTPReply& reply, const HTTPContext& context) {
		const auto guildId = PathId<int64_t>(context.path, 2);
		if (!guildId || *guildId <= 0) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid guild ID");
		const auto guild = Database::Get()->GetGuild(*guildId);
		RenderPage(reply, context, "chat_history.jinja2", "guilds", { {"mode", "guild"}, {"id", std::to_string(*guildId)},
			{"title", guild ? guild->name + " guild chat" : "Guild " + std::to_string(*guildId) + " chat"} });
	});

	Route(eHTTPMethod::GET, "/chat_log/teams", Perm("chat_private"), "Team chat, by team", [](HTTPReply& reply, const HTTPContext& context) {
		RenderPage(reply, context, "chat_history.jinja2", "chat_teams", { {"mode", "teams"}, {"id", ""}, {"title", "Team Chat"} });
	});

	Route(eHTTPMethod::GET, "/characters/:id/whispers", Perm("chat_dms"), "A character's whispers, by conversation", [](HTTPReply& reply, const HTTPContext& context) {
		const auto characterId = PathId<LWOOBJID>(context.path, 1);
		if (!characterId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid character ID");
		const auto name = CharacterName(*characterId);
		if (!name) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Character not found");
		RenderPage(reply, context, "chat_history.jinja2", "characters", { {"mode", "whispers"}, {"id", std::to_string(*characterId)},
			{"title", *name + "'s whispers"}, {"character_name", *name} });
	});

	Route(eHTTPMethod::POST, "/api/chat/send", Perm("chat_send"),
		"Post in players' chat. Body: {message (1-300 characters), name (who it's from, e.g. a Discord user; default your account), label (shown in brackets, "
		"default Web, e.g. Discord), zone (optional: only that zone), instance (optional, with zone: only that world server)}. Shown as \"[label] name\" so it can't pass for a player. Runs in the background: {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			std::string message = body->value("message", "");
			std::erase_if(message, [](char c) { return c == '\r' || c == '\n'; });
			if (message.empty() || message.size() > 300) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The message is 1 to 300 characters");
			std::string label = body->value("label", "Web");
			static const std::regex labelPattern("^[A-Za-z0-9 _-]{1,16}$");
			if (!std::regex_match(label, labelPattern)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The label is up to 16 letters, digits, spaces, - or _");
			std::string name = body->value("name", context.authenticatedUser);
			std::erase_if(name, [](char c) { return c == '\r' || c == '\n' || c == '[' || c == ']'; });
			if (name.empty() || name.size() > 32) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The name is 1 to 32 characters");
			const auto zone = body->value("zone", 0u);
			const int32_t instance = body->value("instance", -1);

			PlayerActionRequest request;
			request.action = ePlayerAction::CHAT_MESSAGE;
			request.name = "[" + label + "] " + name;
			request.text = message;
			request.zoneId = static_cast<LWOMAPID>(zone);
			request.instanceId = zone ? instance : -1;

			IChatLog::ChatMessage entry;
			entry.time = static_cast<int64_t>(std::time(nullptr));
			entry.channel = "web";
			entry.senderName = request.name;
			entry.accountId = context.accountId;
			entry.zoneId = zone;
			entry.instanceId = zone && instance >= 0 ? static_cast<uint32_t>(instance) : 0;
			entry.message = message;
			Database::Get()->InsertChatMessage(entry);

			const auto requestId = PlayerActions::Request(request, context.accountId, [](const PlayerActionResult& result) {
				return PlayerActions::Outcome{ true, result.affected ? "Shown to " + std::to_string(result.affected) + " player(s)" : "Nobody saw it: no one is in a world it went to, or the chat filter stopped it" };
			});
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
