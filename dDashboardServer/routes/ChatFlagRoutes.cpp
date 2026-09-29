#include "ChatFlagRoutes.h"

#include <algorithm>
#include <ctime>
#include <set>

#include "ChatHistory.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Database.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr size_t MAX_MESSAGES = 100;   // flagged at once
	constexpr uint32_t CONTEXT = 10;        // messages kept before and after
	constexpr size_t MAX_NOTE = 2000;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	ChatHistory::Access AccessOf(const HTTPContext& context) {
		return { Can(context, "chat_private"), Can(context, "chat_dms") };
	}

	std::optional<IChatLog::ChatMessage> FindMessage(uint64_t id) {
		if (id == 0) return std::nullopt;
		IChatLog::ChatQuery q;
		q.includePrivate = true;
		q.includeWhispers = true;
		q.afterId = id - 1;
		q.limit = 1;
		const auto found = Database::Get()->GetChatMessages(q);
		if (found.empty() || found.front().id != id) return std::nullopt;
		return found.front();
	}

	// The list's view of a flag: what it's about, without the copy of the chat
	nlohmann::json FlagJson(const IChatFlags::ChatFlag& f, const ChatHistory::Access& access) {
		const bool readable = ChatHistory::CanRead(f.channel, access);
		return { {"id", f.id}, {"created_at", f.createdAt}, {"created_by", f.createdBy}, {"status", f.status}, {"channel", f.channel},
			{"character_id", std::to_string(f.characterId)}, {"character_name", f.characterName}, {"account_id", f.accountId},
			{"first_message_id", f.firstMessageId}, {"last_message_id", f.lastMessageId}, {"first_time", f.firstTime}, {"last_time", f.lastTime},
			{"excerpt", readable ? f.excerpt : ""}, {"redacted", !readable}, {"note", f.note}, {"player_report_id", f.playerReportId},
			{"updated_at", f.updatedAt}, {"updated_by", f.updatedBy} };
	}

	void Event(const HTTPContext& context, uint64_t flagId, const std::string& action, const std::string& detail) {
		Database::Get()->InsertChatFlagEvent({ 0, flagId, Now(), context.accountId, context.authenticatedUser, action, detail });
	}

	std::string Trimmed(std::string text, size_t max) {
		std::erase_if(text, [](char c) { return c == '\r'; });
		if (text.size() > max) text.resize(max);
		return text;
	}
}

void ChatFlagRoutes::RegisterRoutes() {
	Route(eHTTPMethod::GET, "/chat_flags", Perm("chat_flag"), "Chat flags: chat staff flagged for review", [](HTTPReply& reply, const HTTPContext& context) {
		RenderPage(reply, context, "chat_flags.jinja2", "chat_flags");
	});

	Route(eHTTPMethod::POST, "/api/chat/flags", Perm("chat_flag"),
		"Flag chat for review. Body: {message_ids (1-100, from one conversation), character (optional: which sender it is about; default the first), note}. "
		"The flag keeps a copy of the messages and the 10 before and after them in the conversation. Returns {id}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->contains("message_ids") || !(*body)["message_ids"].is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "message_ids is a list of message IDs");
			std::set<uint64_t> ids;
			for (const auto& value : (*body)["message_ids"]) {
				const auto id = value.is_number_unsigned() ? std::optional(value.get<uint64_t>()) : value.is_string() ? GeneralUtils::TryParse<uint64_t>(value.get<std::string>()) : std::nullopt;
				if (!id || *id == 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid message ID");
				ids.insert(*id);
			}
			if (ids.empty() || ids.size() > MAX_MESSAGES) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Flag 1 to 100 messages at once");
			const auto access = AccessOf(context);
			std::vector<IChatLog::ChatMessage> flagged;
			for (const auto id : ids) {
				const auto message = FindMessage(id);
				if (!message) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Message " + std::to_string(id) + " is not in the chat log (chat is kept for log_chat_days)");
				if (!ChatHistory::CanRead(message->channel, access)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Reading " + message->channel + " chat needs another permission");
				if (!flagged.empty() && !ChatHistory::SameConversation(flagged.front(), *message)) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The messages are from different conversations; flag each conversation on its own");
				}
				flagged.push_back(*message);
			}
			const auto chosen = GeneralUtils::TryParse<LWOOBJID>(body->value("character", std::string{})).value_or(0);
			const auto subject = ChatHistory::Subject(flagged, chosen);

			// The chat around them, from the same conversation
			auto beforeQuery = ChatHistory::ConversationQuery(flagged.front(), access);
			beforeQuery.beforeId = flagged.front().id;
			beforeQuery.newestFirst = true;
			beforeQuery.limit = CONTEXT;
			auto before = Database::Get()->GetChatMessages(beforeQuery);
			std::reverse(before.begin(), before.end());
			auto afterQuery = ChatHistory::ConversationQuery(flagged.front(), access);
			afterQuery.afterId = flagged.back().id;
			afterQuery.limit = CONTEXT;
			const auto after = Database::Get()->GetChatMessages(afterQuery);
			// Messages between the first and last flagged one that weren't picked stay in the copy too
			auto betweenQuery = ChatHistory::ConversationQuery(flagged.front(), access);
			betweenQuery.afterId = flagged.front().id;
			betweenQuery.beforeId = flagged.back().id;
			betweenQuery.limit = 500;
			for (const auto& m : Database::Get()->GetChatMessages(betweenQuery)) {
				if (!ids.contains(m.id)) before.push_back(m);
			}

			IChatFlags::ChatFlag flag;
			flag.createdAt = Now();
			flag.createdById = context.accountId;
			flag.createdBy = context.authenticatedUser;
			flag.channel = flagged.front().channel;
			if (subject) {
				flag.characterId = subject->senderId;
				flag.characterName = subject->senderName;
				flag.accountId = subject->accountId;
			}
			flag.firstMessageId = flagged.front().id;
			flag.lastMessageId = flagged.back().id;
			flag.firstTime = flagged.front().time;
			flag.lastTime = flagged.back().time;
			flag.excerpt = ChatHistory::Excerpt(flagged);
			flag.note = Trimmed(body->value("note", std::string{}), MAX_NOTE);
			flag.messages = ChatHistory::Snapshot(before, flagged, after).dump();
			const auto id = Database::Get()->InsertChatFlag(flag, { ids.begin(), ids.end() });
			Event(context, id, "created", std::to_string(ids.size()) + " message(s)" + (flag.note.empty() ? "" : ": " + flag.note));
			Audit(context, "chat_flag", "Flagged " + std::to_string(ids.size()) + " " + flag.channel + " message(s) as chat flag " + std::to_string(id) +
				(flag.characterName.empty() ? "" : " about " + flag.characterName), subject ? AuditTarget::Account(subject->accountId) : AuditTarget{});
			BroadcastTableChanged("chat_flags", std::to_string(id));
			JsonSuccess(reply, { {"id", id} });
		});

	ReadRoute(eHTTPMethod::POST, "/api/tables/chat_flags", Perm("chat_flag"),
		"Chat flags (DataTables), newest first. Body adds {status (open, actioned, dismissed; empty: any), character (name or ID), account}. "
		"Flags on team and guild chat need chat_private, on whispers chat_dms",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto access = AccessOf(context);
			IChatFlags::ChatFlagQuery q;
			q.includePrivate = access.group;
			q.includeWhispers = access.whispers;
			q.status = ChatHistory::ParseStatus(body->value("status", std::string{})).value_or("");
			const auto character = body->value("character", std::string{});
			if (!character.empty()) q.characterId = ResolveCharacter(character).value_or(-1);
			q.accountId = body->contains("account") && (*body)["account"].is_number_unsigned() ? (*body)["account"].get<uint32_t>()
				: GeneralUtils::TryParse<uint32_t>(body->value("account", std::string{})).value_or(0);
			q.offset = request->start;
			q.limit = std::clamp<uint32_t>(request->length, 1, 200);
			IChatFlags::ChatFlagQuery all;
			all.includePrivate = q.includePrivate;
			all.includeWhispers = q.includeWhispers;
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& flag : Database::Get()->GetChatFlags(q)) rows.push_back(FlagJson(flag, access));
			JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", Database::Get()->CountChatFlags(all)},
				{"recordsFiltered", Database::Get()->CountChatFlags(q)}, {"data", rows} });
		});

	Route(eHTTPMethod::GET, "/api/chat/flags/:id", Perm("chat_flag"),
		"One chat flag: {flag, messages (the copy of the chat, flagged ones marked; text of channels you may not read left out), events (its history, oldest first)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 3);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid flag ID");
			const auto flag = Database::Get()->GetChatFlag(*id);
			if (!flag) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such chat flag");
			const auto access = AccessOf(context);
			auto snapshot = nlohmann::json::parse(flag->messages, nullptr, false);
			nlohmann::json events = nlohmann::json::array();
			for (const auto& e : Database::Get()->GetChatFlagEvents(*id)) {
				events.push_back({ {"id", e.id}, {"time", e.time}, {"actor", e.actor}, {"action", e.action}, {"detail", e.detail} });
			}
			if (flag->channel == "whisper" && access.whispers) {
				Audit(context, "chat_dms_view", "Whispers in chat flag " + std::to_string(*id), AuditTarget::Character(flag->characterId));
			}
			JsonSuccess(reply, { {"flag", FlagJson(*flag, access)}, {"messages", ChatHistory::RedactSnapshot(snapshot, access)}, {"events", events},
				{"canReview", Can(context, "chat_flag_review")} });
		});

	Route(eHTTPMethod::POST, "/api/chat/flags/:id", Perm("chat_flag_review"),
		"Review a chat flag. Body (each optional): {status (open, actioned, dismissed), note (replaces it), player_report_id (0 to unlink), comment (added to its history)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 3);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid flag ID");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto flag = Database::Get()->GetChatFlag(*id);
			if (!flag) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such chat flag");
			// Only chat the reviewer may read
			if (!ChatHistory::CanRead(flag->channel, AccessOf(context))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Reviewing " + flag->channel + " chat needs another permission");

			std::string status = flag->status;
			if (body->contains("status")) {
				const auto parsed = ChatHistory::ParseStatus(body->value("status", std::string{}));
				if (!parsed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "status is open, actioned or dismissed");
				status = *parsed;
			}
			const std::string note = body->contains("note") ? Trimmed(body->value("note", std::string{}), MAX_NOTE) : flag->note;
			uint64_t report = flag->playerReportId;
			if (body->contains("player_report_id")) {
				const auto& value = (*body)["player_report_id"];
				const auto parsed = value.is_number_unsigned() ? std::optional(value.get<uint64_t>()) : value.is_string() ? GeneralUtils::TryParse<uint64_t>(value.get<std::string>()) : std::nullopt;
				if (!parsed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "player_report_id is a number");
				report = *parsed;
			}
			const auto comment = Trimmed(body->value("comment", std::string{}), MAX_NOTE);

			const auto action = ChatHistory::StatusAction(flag->status, status);
			const bool changed = !action.empty() || note != flag->note || report != flag->playerReportId;
			if (!changed && comment.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Nothing to change");
			if (changed) Database::Get()->UpdateChatFlag(*id, status, note, report, Now(), context.authenticatedUser);
			if (!action.empty()) Event(context, *id, action, comment);
			if (note != flag->note) Event(context, *id, "note", note);
			if (report != flag->playerReportId) Event(context, *id, "report", report ? "Player report " + std::to_string(report) : "No player report");
			if (action.empty() && !comment.empty()) Event(context, *id, "comment", comment);

			std::string what = "Chat flag " + std::to_string(*id) + ":";
			if (!action.empty()) what += " " + action;
			if (note != flag->note) what += " note changed";
			if (report != flag->playerReportId) what += " report " + std::to_string(report);
			if (!comment.empty()) what += " (" + comment + ")";
			Audit(context, "chat_flag_review", what, AuditTarget::Account(flag->accountId));
			BroadcastTableChanged("chat_flags", std::to_string(*id));
			JsonSuccess(reply);
		});
}
