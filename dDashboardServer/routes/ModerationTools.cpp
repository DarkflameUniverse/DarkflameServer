#include "ModerationTools.h"
#include "ChatFilterWords.h"

#include <ctime>
#include <fstream>
#include <map>

#include "RouteUtils.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "GameLabels.h"
#include "PlayerActions.h"
#include "PlayerAction.h"
#include "Strikes.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Web.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "eAccountLink.h"
#include "ePlayerReportKind.h"
#include "ePlayerReportStatus.h"
#include "magic_enum.hpp"

using namespace RouteUtils;

namespace {
	constexpr uint32_t MAX_REPORTS = 200;
	// The chat filter's files, as the servers load them: the allowed words from the client's res folder, the blocked
	// words (only their hashes) next to the servers
	constexpr const char* ALLOW_FILE = "chatplus_en_us.txt";
	constexpr const char* BLOCK_FILE = "blocklist.dcf";
	constexpr uint32_t FILE_WORDS_PAGE = 200;
	// Recent chat searched when checking what a word would change
	constexpr uint32_t CHECK_MESSAGES = 1000;

	std::string Trimmed(std::string text, size_t max) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text.substr(0, max);
	}

	template<typename Enum> nlohmann::json EnumLabels() {
		nlohmann::json list = nlohmann::json::array();
		for (const auto value : magic_enum::enum_values<Enum>()) {
			list.push_back({ {"value", static_cast<int>(value)}, {"key", std::string(magic_enum::enum_name(value))}, {"name", GameLabels::Name(value)} });
		}
		return list;
	}

	// Names looked up once per request
	struct Names {
		std::map<LWOOBJID, std::string> characters;
		std::map<uint32_t, std::string> accounts;

		const std::string& Character(LWOOBJID id) {
			if (!characters.contains(id)) {
				const auto info = id ? Database::Get()->GetCharacterInfo(id) : std::nullopt;
				characters[id] = info ? info->name : "";
			}
			return characters[id];
		}
		const std::string& Account(uint32_t id) {
			if (!accounts.contains(id)) {
				const auto account = id ? Database::Get()->GetAccountById(id) : nlohmann::json();
				accounts[id] = account.is_object() ? account.value("name", "") : "";
			}
			return accounts[id];
		}
	};

	nlohmann::json ReportJson(const IModeration::PlayerReport& r, Names& names) {
		const auto kind = magic_enum::enum_cast<ePlayerReportKind>(r.kind);
		const auto status = static_cast<ePlayerReportStatus>(r.status);
		const auto& zones = ZoneNames();
		const auto zone = std::to_string(r.zoneId);
		std::string propertyName;
		if (r.propertyId) {
			if (const auto property = Database::Get()->GetPropertyInfo(r.propertyId)) propertyName = property->name;
		}
		return {
			{"id", r.id}, {"created_at", r.createdAt}, {"kind", r.kind}, {"kind_name", kind ? GameLabels::Name(*kind) : r.kind},
			{"reporter_id", std::to_string(r.reporterId)}, {"reporter_name", names.Character(r.reporterId)}, {"reporter_account_id", r.reporterAccountId},
			{"object_id", std::to_string(r.objectId)}, {"object_lot", r.objectLot}, {"object_name", r.objectLot ? ClientAssets::ItemName(r.objectLot) : ""},
			{"target_character_id", std::to_string(r.targetCharacterId)}, {"target_name", names.Character(r.targetCharacterId)},
			{"target_account_id", r.targetAccountId}, {"target_account_name", names.Account(r.targetAccountId)},
			{"property_id", std::to_string(r.propertyId)}, {"property_name", propertyName},
			{"zone_id", r.zoneId}, {"zone_name", zones.contains(zone) ? zones[zone] : nlohmann::json("")}, {"instance_id", r.instanceId}, {"clone_id", r.cloneId},
			{"body", r.body}, {"status", r.status}, {"status_key", std::string(magic_enum::enum_name(status))}, {"status_name", GameLabels::Name(status)},
			{"handled_by", r.handledBy}, {"handled_at", r.handledAt}, {"resolution", r.resolution}
		};
	}

	// Load a report for staff acting on it; replies with the error when it can't be acted on
	std::optional<IModeration::PlayerReport> OpenReport(const HTTPContext& context, HTTPReply& reply) {
		const auto id = PathId<uint64_t>(context.path, 2);
		const auto report = id ? Database::Get()->GetPlayerReport(*id) : std::nullopt;
		if (!report) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such report");
			return std::nullopt;
		}
		if (report->status != static_cast<uint8_t>(ePlayerReportStatus::OPEN)) {
			JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This report was already handled by " + report->handledBy);
			return std::nullopt;
		}
		return report;
	}

	std::string ReportSubject(const IModeration::PlayerReport& report) {
		return "Report #" + std::to_string(report.id);
	}

	// Load the words again in every running world; the reply's request id shows how many did
	uint32_t ReloadWorlds(uint32_t requester) {
		PlayerActionRequest request;
		request.action = ePlayerAction::RELOAD_CHAT_FILTER;
		return PlayerActions::Request(request, requester, [](const PlayerActionResult& result) {
			return PlayerActions::Outcome{ true, result.affected ? "The chat filter was updated in " + std::to_string(result.affected) + " world(s)"
				: "No world is running; they'll use the words when they start" };
		});
	}

	void RegisterPlayerReportRoutes() {
		Route(eHTTPMethod::GET, "/player_reports", Perm("player_reports_view"), "Reports players sent from the game",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "player_reports.jinja2", "player_reports"); });

		Route(eHTTPMethod::GET, "/api/player_reports", Perm("player_reports_view"),
			"Player reports, newest first. Query: status (ePlayerReportStatus value; missing: any), account (reported account), limit, offset. "
			"Returns {reports, total, kinds, statuses}",
			[](HTTPReply& reply, const HTTPContext& context) {
				IModeration::PlayerReportQuery query;
				query.status = GeneralUtils::TryParse<int16_t>(QueryValue(context.queryString, "status")).value_or(-1);
				query.accountId = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "account")).value_or(0);
				query.limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(50), 1, MAX_REPORTS);
				query.offset = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "offset")).value_or(0);
				Names names;
				nlohmann::json reports = nlohmann::json::array();
				for (const auto& report : Database::Get()->GetPlayerReports(query)) reports.push_back(ReportJson(report, names));
				JsonSuccess(reply, { {"reports", reports}, {"total", Database::Get()->CountPlayerReports(query)},
					{"kinds", EnumLabels<ePlayerReportKind>()}, {"statuses", EnumLabels<ePlayerReportStatus>()},
					{"canManage", Can(context, "player_reports_manage")} });
			});

		Route(eHTTPMethod::POST, "/api/player_reports/:id/action", Perm("player_reports_manage"),
			"Mark a report acted on. Body: {reason (what was done), strike (true: also a strike on the reported account)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context).value_or(nlohmann::json::object());
				const auto report = OpenReport(context, reply);
				if (!report) return;
				const auto strike = Strikes::Requested(context, body, reply);
				if (!strike) return;
				if (*strike && report->targetAccountId == 0) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The game didn't say whose this is, so there's no account to give a strike to; nothing was done");
				}
				if (*strike && !AuthorizeAccountAction(context, report->targetAccountId, reply, eAccountAction::MODERATION)) return;
				const auto reason = Trimmed(body.value("reason", ""), 500);
				Database::Get()->SetPlayerReportStatus(report->id, static_cast<uint8_t>(ePlayerReportStatus::ACTIONED), context.authenticatedUser, reason,
					static_cast<int64_t>(std::time(nullptr)));
				Audit(context, "action_player_report", ReportSubject(*report) + (reason.empty() ? "" : ": " + reason),
					report->targetAccountId ? AuditTarget::Account(report->targetAccountId) : AuditTarget{});
				nlohmann::json result{ {"message", ReportSubject(*report) + " marked acted on" + std::string(*strike ? ", with a strike" : "")} };
				if (*strike) {
					Strikes::Give(context, report->targetAccountId, report->targetCharacterId, eStrikeSource::PLAYER_REPORT, ReportSubject(*report),
						reason.empty() ? report->body.substr(0, 200) : reason).Into(result);
				}
				BroadcastTableChanged("player_reports", std::to_string(report->id));
				JsonSuccess(reply, result);
			});

		Route(eHTTPMethod::POST, "/api/player_reports/:id/dismiss", Perm("player_reports_manage"), "Dismiss a report: nothing to act on. Body: {reason (optional)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context).value_or(nlohmann::json::object());
				const auto report = OpenReport(context, reply);
				if (!report) return;
				const auto reason = Trimmed(body.value("reason", ""), 500);
				Database::Get()->SetPlayerReportStatus(report->id, static_cast<uint8_t>(ePlayerReportStatus::DISMISSED), context.authenticatedUser, reason,
					static_cast<int64_t>(std::time(nullptr)));
				Audit(context, "dismiss_player_report", ReportSubject(*report) + (reason.empty() ? "" : ": " + reason),
					report->targetAccountId ? AuditTarget::Account(report->targetAccountId) : AuditTarget{});
				BroadcastTableChanged("player_reports", std::to_string(report->id));
				JsonSuccess(reply, { {"message", ReportSubject(*report) + " dismissed"} });
			});
	}

	void RegisterLinkedAccountRoutes() {
		Route(eHTTPMethod::GET, "/api/accounts/:id/links", Perm("accounts_links"),
			"Other accounts sharing a play key, email address or login address with this one: {accounts: [{account_id, name, gm_level, banned, links: [{link, name, shared, last_seen}]}], loginAddresses}. "
			"Addresses themselves are never returned",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = PathId<uint32_t>(context.path, 2);
				if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid account");
				// One entry per other account, with every signal it shares, in the order first found
				nlohmann::json accounts = nlohmann::json::array();
				std::map<uint32_t, size_t> index;
				for (const auto& linked : Database::Get()->GetLinkedAccounts(*accountId)) {
					if (!index.contains(linked.accountId)) {
						index[linked.accountId] = accounts.size();
						accounts.push_back({ {"account_id", linked.accountId}, {"name", linked.name}, {"gm_level", linked.gmLevel}, {"banned", linked.banned},
							{"links", nlohmann::json::array()} });
					}
					const auto link = static_cast<eAccountLink>(linked.link);
					accounts[index[linked.accountId]]["links"].push_back({ {"link", std::string(magic_enum::enum_name(link))}, {"name", GameLabels::Name(link)},
						{"shared", linked.shared}, {"last_seen", linked.lastSeen} });
				}
				JsonSuccess(reply, { {"accounts", accounts}, {"loginAddresses", Database::Get()->CountLoginAddresses(*accountId)},
					{"reports", Database::Get()->CountPlayerReports({ .status = -1, .accountId = *accountId })},
					{"openReports", Database::Get()->CountPlayerReports({ .status = static_cast<int16_t>(ePlayerReportStatus::OPEN), .accountId = *accountId })},
					{"canSeeReports", Can(context, "player_reports_view")} });
			});
	}

	std::vector<std::string> AllowFileWords() {
		const auto text = ClientAssets::ReadResFile(ALLOW_FILE);
		return text ? ModerationTools::FileWords(*text) : std::vector<std::string>{};
	}

	std::optional<std::vector<size_t>> BlockFileHashes() {
		std::ifstream in(BLOCK_FILE, std::ios::binary);
		if (!in) return std::nullopt;
		return ModerationTools::DcfHashes(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
	}

	// The dashboard's own lists by word: true allowed, false blocked
	std::map<std::string, bool> DashboardWords() {
		std::map<std::string, bool> words;
		for (const auto& word : Database::Get()->GetChatFilterWords()) words[word.word] = word.allowed;
		return words;
	}

	void RegisterChatFilterRoutes() {
		Route(eHTTPMethod::GET, "/chat_filter", Perm("chat_filter_manage"), "Words the chat filter allows and stops",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "chat_filter.jinja2", "chat_filter"); });

		Route(eHTTPMethod::GET, "/api/chat_filter", Perm("chat_filter_manage"), "Words staff added to the chat filter: {words: [{word, allowed, added_by, added_at}]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json words = nlohmann::json::array();
				for (const auto& word : Database::Get()->GetChatFilterWords()) {
					words.push_back({ {"word", word.word}, {"allowed", word.allowed}, {"added_by", word.addedBy}, {"added_at", word.addedAt} });
				}
				JsonSuccess(reply, { {"words", words} });
			});

		Route(eHTTPMethod::GET, "/api/chat_filter/files", Perm("chat_filter_manage"),
			"The words of the filter's files: the allowed words of chatplus_en_us.txt (from the client) matching ?search=, 200 from ?start=, "
			"each with what the dashboard's lists say about it, and how many hashed words blocklist.dcf has (its words can't be listed)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto all = AllowFileWords();
				const auto search = ModerationTools::FilterWord(QueryValue(context.queryString, "search")).value_or("");
				const auto start = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "start")).value_or(0);
				const auto dashboard = DashboardWords();
				nlohmann::json words = nlohmann::json::array();
				uint32_t matched = 0;
				for (const auto& word : all) {
					if (!search.empty() && word.find(search) == std::string::npos) continue;
					if (matched++ < start || words.size() >= FILE_WORDS_PAGE) continue;
					const auto it = dashboard.find(word);
					words.push_back({ {"word", word}, {"dashboard", it == dashboard.end() ? nlohmann::json(nullptr) : nlohmann::json(it->second ? "allowed" : "blocked")} });
				}
				const auto blocked = BlockFileHashes();
				uint32_t imported = 0;
				for (const auto& word : all) if (dashboard.contains(word)) imported++;
				JsonSuccess(reply, { {"allowFile", ALLOW_FILE}, {"allowFileFound", !all.empty()}, {"allowTotal", all.size()}, {"matched", matched},
					{"start", start}, {"pageSize", FILE_WORDS_PAGE}, {"words", words}, {"onDashboard", imported},
					{"blockFile", BLOCK_FILE}, {"blockFileFound", blocked.has_value()}, {"blockTotal", blocked ? blocked->size() : 0} });
			});

		Route(eHTTPMethod::GET, "/api/chat_filter/lookup", Perm("chat_filter_manage"),
			"Where a word stands: in the allowed words file, in the blocked words file (by its hash), and on the dashboard's lists. Query: word",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto word = ModerationTools::FilterWord(QueryValue(context.queryString, "word"));
				if (!word) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Type one word (no spaces), up to 64 characters");
				const auto all = AllowFileWords();
				const auto blocked = BlockFileHashes();
				const auto dashboard = DashboardWords();
				const auto it = dashboard.find(*word);
				JsonSuccess(reply, { {"word", *word}, {"inAllowFile", std::binary_search(all.begin(), all.end(), *word)},
					{"inBlockFile", blocked && std::find(blocked->begin(), blocked->end(), ModerationTools::WordHash(*word)) != blocked->end()},
					{"dashboard", it == dashboard.end() ? nlohmann::json(nullptr) : nlohmann::json(it->second ? "allowed" : "blocked")} });
			});

		Route(eHTTPMethod::POST, "/api/chat_filter/import", Perm("chat_filter_manage"),
			"Copy the words of chatplus_en_us.txt into the dashboard's allowed list, so they can be edited here; words already on either of "
			"the dashboard's lists are left as they are",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto all = AllowFileWords();
				if (all.empty()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, std::string("Could not read ") + ALLOW_FILE + " from the client (client_location)");
				const auto dashboard = DashboardWords();
				const auto now = static_cast<int64_t>(std::time(nullptr));
				uint32_t added = 0;
				DatabaseTransaction transaction(*Database::Get());
				for (const auto& word : all) {
					// Only words the filter could compare (the file's odd lines with spaces or punctuation stay in the file only)
					const auto filterWord = ModerationTools::FilterWord(word);
					if (!filterWord || *filterWord != word || dashboard.contains(word)) continue;
					Database::Get()->SetChatFilterWord({ word, true, context.authenticatedUser, now });
					added++;
				}
				transaction.Commit();
				Audit(context, "chat_filter_import", "Copied " + std::to_string(added) + " words from " + ALLOW_FILE + " into the chat filter's allowed list");
				BroadcastTableChanged("chat_filter");
				JsonSuccess(reply, { {"message", "Copied " + std::to_string(added) + " words (" + std::to_string(all.size() - added) + " were already here or can't be used)"},
					{"added", added}, {"requestId", ReloadWorlds(context.accountId)} });
			});

		Route(eHTTPMethod::POST, "/api/chat_filter/words", Perm("chat_filter_manage"),
			"Allow or block a word (or move it to the other list); running worlds pick it up at once. Body: {word, allowed: bool}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto word = ModerationTools::FilterWord(body->value("word", ""));
				if (!word) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Type one word (no spaces), up to 64 characters");
				const bool allowed = body->value("allowed", false);
				Database::Get()->SetChatFilterWord({ *word, allowed, context.authenticatedUser, static_cast<int64_t>(std::time(nullptr)) });
				Audit(context, allowed ? "chat_filter_allow" : "chat_filter_block", (allowed ? "Allowed \"" : "Blocked \"") + *word + "\" in chat");
				BroadcastTableChanged("chat_filter");
				JsonSuccess(reply, { {"message", "\"" + *word + "\" is now " + (allowed ? "allowed" : "blocked")}, {"requestId", ReloadWorlds(context.accountId)} });
			});

		Route(eHTTPMethod::POST, "/api/chat_filter/words/delete", Perm("chat_filter_manage"),
			"Remove a word added here; the filter's files decide about it again. Body: {word}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				const auto word = body ? ModerationTools::FilterWord(body->value("word", "")) : std::nullopt;
				if (!word) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid word");
				if (!Database::Get()->DeleteChatFilterWord(*word)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That word isn't on either list");
				Audit(context, "chat_filter_remove", "Removed \"" + *word + "\" from the chat filter's lists");
				BroadcastTableChanged("chat_filter");
				JsonSuccess(reply, { {"message", "\"" + *word + "\" removed"}, {"requestId", ReloadWorlds(context.accountId)} });
			});

		Route(eHTTPMethod::POST, "/api/chat_filter/reload", Perm("chat_filter_manage"), "Load the words again in every running world",
			[](HTTPReply& reply, const HTTPContext& context) {
				Audit(context, "chat_filter_reload", "Reloaded the chat filter's words in running worlds");
				JsonSuccess(reply, { {"requestId", ReloadWorlds(context.accountId)} });
			});

		Route(eHTTPMethod::GET, "/api/chat_filter/check", Perm("chat_filter_manage"),
			"Recent chat a word would change. Query: word, allowed (1: messages the filter stopped that contain it; otherwise messages it let through "
			"that blocking it would have stopped). Searches the newest 1000 messages containing the text. Also needs chat_view; private chat only with chat_private",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!Can(context, "chat_view")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Reading chat needs the chat_view permission");
				const auto word = ModerationTools::FilterWord(QueryValue(context.queryString, "word"));
				if (!word) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Type one word (no spaces), up to 64 characters");
				const bool allowed = QueryValue(context.queryString, "allowed") == "1";
				IChatLog::ChatQuery query;
				query.search = *word;
				query.includePrivate = Can(context, "chat_private");
				query.blockedOnly = allowed;
				query.newestFirst = true;
				query.limit = CHECK_MESSAGES;
				const auto& zones = ZoneNames();
				nlohmann::json messages = nlohmann::json::array();
				uint32_t searched = 0;
				for (const auto& m : Database::Get()->GetChatMessages(query)) {
					searched++;
					// Only messages from players: web messages aren't filtered, and the filter compares whole words
					if (m.channel == "web" || m.blocked != allowed || !ModerationTools::HasFilterWord(m.message, *word)) continue;
					const auto zone = std::to_string(m.zoneId);
					messages.push_back({ {"id", m.id}, {"time", m.time}, {"channel", m.channel}, {"sender_id", std::to_string(m.senderId)},
						{"sender_name", m.senderName}, {"account_id", m.accountId}, {"zone_id", m.zoneId},
						{"zone_name", zones.contains(zone) ? zones[zone] : nlohmann::json("")}, {"message", m.message}, {"blocked", m.blocked} });
				}
				JsonSuccess(reply, { {"word", *word}, {"allowed", allowed}, {"messages", messages}, {"searched", searched}, {"limit", CHECK_MESSAGES} });
			});
	}
}

void RegisterModerationToolRoutes() {
	RegisterPlayerReportRoutes();
	RegisterLinkedAccountRoutes();
	RegisterChatFilterRoutes();
}
