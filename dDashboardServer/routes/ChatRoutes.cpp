#include "ChatRoutes.h"

#include <ctime>
#include <regex>

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

	bool IsPrivate(const std::string& channel) { return channel == "whisper" || channel == "team"; }

	nlohmann::json MessageJson(const IChatLog::ChatMessage& m) {
		const auto& zones = ZoneNames();
		const auto zone = std::to_string(m.zoneId);
		return { {"id", m.id}, {"time", m.time}, {"channel", m.channel}, {"sender_id", std::to_string(m.senderId)}, {"sender_name", m.senderName},
			{"account_id", m.accountId}, {"recipient_id", std::to_string(m.recipientId)}, {"recipient_name", m.recipientName},
			{"zone_id", m.zoneId}, {"zone_name", zones.contains(zone) ? zones[zone] : nlohmann::json("")}, {"instance_id", m.instanceId},
			{"clone_id", m.cloneId}, {"message", m.message}, {"blocked", m.blocked} };
	}

	// The filters both the API and the Chat Log page take, from a query string or a JSON body
	IChatLog::ChatQuery Query(const HTTPContext& context, const std::function<std::string(const char*)>& get) {
		IChatLog::ChatQuery q;
		q.channel = get("channel");
		// Whispers and team chat need their own permission
		q.includePrivate = Can(context, "chat_private");
		if (IsPrivate(q.channel) && !q.includePrivate) q.channel = "none";
		const auto character = get("character");
		if (!character.empty()) q.characterId = ResolveCharacter(character).value_or(-1);
		q.accountId = GeneralUtils::TryParse<uint32_t>(get("account")).value_or(0);
		q.zoneId = GeneralUtils::TryParse<uint32_t>(get("zone")).value_or(0);
		q.instanceId = GeneralUtils::TryParse<int64_t>(get("instance")).value_or(-1);
		q.search = get("search");
		q.blockedOnly = get("blocked") == "1" || get("blocked") == "true";
		return q;
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
				auto json = MessageJson(message);
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
		"Chat messages, oldest first. Query: after (the last id you have; for bridges polling), limit (max 500), channel (zone, whisper, team, web), "
		"character (name or ID), account, zone, instance (one world server of that zone), search, blocked=1 (only what the chat filter stopped). Whispers and team chat need chat_private. "
		"Returns {messages, last_id}. Live: subscribe to the chat_message WebSocket topic",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto q = Query(context, [&](const char* name) { return QueryValue(context.queryString, name); });
			q.afterId = GeneralUtils::TryParse<uint64_t>(QueryValue(context.queryString, "after")).value_or(0);
			q.limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(100), 1, MAX_PAGE);
			// Without `after`, the newest page (still returned oldest first)
			q.newestFirst = q.afterId == 0;
			auto messages = Database::Get()->GetChatMessages(q);
			if (q.newestFirst) std::reverse(messages.begin(), messages.end());
			nlohmann::json out = nlohmann::json::array();
			uint64_t last = q.afterId;
			for (const auto& m : messages) { out.push_back(MessageJson(m)); last = std::max(last, m.id); }
			JsonSuccess(reply, { {"messages", out}, {"last_id", last} });
		});

	Route(eHTTPMethod::POST, "/api/tables/chat_log", Perm("chat_view"), "The chat log for the Chat Log page (DataTables). Body adds {channel, character, zone, instance, blocked}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
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
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& m : Database::Get()->GetChatMessages(q)) rows.push_back(MessageJson(m));
			JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", Database::Get()->CountChatMessages(all)},
				{"recordsFiltered", Database::Get()->CountChatMessages(q)}, {"data", rows} });
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
