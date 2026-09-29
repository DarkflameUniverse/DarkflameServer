#include "ModeratorHelper.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

#include "AiBudget.h"
#include "ClaudeClient.h"
#include "ModeratorPrompt.h"
#include "RouteUtils.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "GameLabels.h"
#include "PlayerActions.h"
#include "Strikes.h"
#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "ePlayerReportKind.h"
#include "ePlayerReportStatus.h"
#include "magic_enum.hpp"

using namespace RouteUtils;
using ModeratorPrompt::eKind;

namespace {
	// How much history goes in a case
	constexpr uint32_t MAX_CHAT_LINES = 40;
	constexpr uint32_t MAX_STRIKES = 20;
	constexpr uint32_t MAX_HISTORY = 20;
	constexpr uint32_t MAX_EARLIER_REPORTS = 10;

	struct Settings {
		bool enabled{};
		ClaudeClient::Config client;
		AiBudget::Limits limits;
		std::string rules;
		uint32_t chatMinutes{ 10 };
	};

	uint32_t IntSetting(const std::string& key, uint32_t fallback, uint32_t min, uint32_t max) {
		return std::clamp(GeneralUtils::TryParse<uint32_t>(Game::config->GetValue(key)).value_or(fallback), min, max);
	}

	// Main thread only (the config isn't read from the worker)
	Settings ReadSettings() {
		Settings s;
		s.enabled = Game::config->GetValue("ai_helper_enabled") == "1";
		s.client.apiKey = Game::config->GetValue("claude_api_key");
		const auto& base = Game::config->GetValue("claude_api_base");
		if (!base.empty()) s.client.base = base;
		const auto& model = Game::config->GetValue("claude_model");
		if (!model.empty()) s.client.model = model;
		s.client.structuredOutput = Game::config->GetValue("claude_structured_output") != "0";
		s.client.timeoutSeconds = IntSetting("ai_helper_timeout", 60, 5, 600);
		s.client.maxTokens = IntSetting("ai_helper_max_tokens", 2000, 256, 16000);
		s.limits.perMinute = IntSetting("ai_helper_per_minute", 5, 1, 60);
		s.limits.perDay = IntSetting("ai_helper_per_day", 200, 1, 100000);
		s.rules = Game::config->GetValue("ai_helper_rules");
		s.chatMinutes = IntSetting("ai_helper_chat_minutes", 10, 1, 1440);
		return s;
	}

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	// ---- The budget, seeded from the database once so a restart doesn't reset the day ----

	AiBudget g_Budget;
	bool g_BudgetSeeded = false;

	AiBudget& Budget() {
		if (!g_BudgetSeeded) {
			const auto now = Now();
			g_Budget.Seed(now, Database::Get()->GetAiUsageSince(AiBudget::Day(now) * 86400).requests);
			g_BudgetSeeded = true;
		}
		return g_Budget;
	}

	// ---- The worker: one request at a time, so a slow API never ties up the web thread ----

	struct Job {
		uint32_t requestId{};
		ClaudeClient::Config config;
		ModeratorPrompt::Prompt prompt;
		ModeratorPrompt::eKind kind{};
		// Kept for the main thread when the job is done
		HTTPContext context;
		int64_t itemId{};
		std::string fingerprint;
		nlohmann::json caseJson;
		uint32_t accountId{};
		LWOOBJID characterId{};
		// Filled in by the worker
		ClaudeClient::Result result;
		ModeratorPrompt::Parsed parsed;
	};

	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<Job> g_Jobs;
	std::deque<Job> g_Done;
	bool g_Stopping = false;
	std::map<std::string, uint32_t> g_InFlight; // kind:item -> request id, main thread only

	void WorkerLoop() {
		while (true) {
			Job job;
			{
				std::unique_lock lock(g_Mutex);
				g_Wake.wait(lock, [] { return g_Stopping || !g_Jobs.empty(); });
				if (g_Stopping) return;
				job = std::move(g_Jobs.front());
				g_Jobs.pop_front();
			}
			try {
				ClaudeClient::Request request{ job.prompt.system, job.prompt.user, job.prompt.schema };
				// Waits between retries end early, and no new try starts, when the dashboard shuts down
				const auto transport = [](const std::string& url, const std::vector<std::string>& headers, const std::string& body, uint32_t timeout) {
					{
						std::lock_guard lock(g_Mutex);
						if (g_Stopping) return ClaudeClient::HttpResponse{ 0, "", "", "the dashboard is shutting down" };
					}
					return ClaudeClient::CurlTransport(url, headers, body, timeout);
				};
				const auto sleep = [](uint32_t ms) {
					std::unique_lock lock(g_Mutex);
					g_Wake.wait_for(lock, std::chrono::milliseconds(ms), [] { return g_Stopping; });
				};
				job.result = ClaudeClient::Send(job.config, request, transport, sleep);
				if (job.result.ok) job.parsed = ModeratorPrompt::Parse(job.result.text, job.kind, job.prompt);
			} catch (const std::exception& ex) {
				job.result.ok = false;
				job.result.error = ClaudeClient::eError::BAD_RESPONSE;
				job.result.message = std::string("The helper failed: ") + ex.what();
			}
			std::lock_guard lock(g_Mutex);
			g_Done.push_back(std::move(job));
		}
	}

	void Enqueue(Job job) {
		if (!g_Worker.joinable()) {
			g_Stopping = false;
			g_Worker = std::thread(WorkerLoop);
		}
		{
			std::lock_guard lock(g_Mutex);
			g_Jobs.push_back(std::move(job));
		}
		g_Wake.notify_one();
	}

	// ---- What staff get back ----

	nlohmann::json BudgetJson(const Settings& settings) {
		auto& budget = Budget();
		const auto now = Now();
		return { {"used_today", budget.UsedToday(now)}, {"per_day", settings.limits.perDay}, {"remaining_today", budget.RemainingToday(settings.limits, now)},
			{"per_minute", settings.limits.perMinute}, {"resets_at", AiBudget::NextDayStart(now)} };
	}

	nlohmann::json RowJson(const IAiSuggestions::AiSuggestion& row, bool cached, uint32_t accountId, LWOOBJID characterId) {
		nlohmann::json json{
			{"id", row.id}, {"kind", row.kind}, {"item_id", std::to_string(row.itemId)}, {"cached", cached}, {"created_at", row.createdAt},
			{"requested_by", row.requestedBy}, {"model", row.model}, {"input_tokens", row.inputTokens}, {"output_tokens", row.outputTokens},
			{"account_id", accountId}, {"character_id", std::to_string(characterId)},
			{"suggestion", nlohmann::json::parse(row.suggestion.empty() ? "null" : row.suggestion, nullptr, false)},
			{"case", nlohmann::json::parse(row.context.empty() ? "null" : row.context, nullptr, false)}
		};
		return json;
	}

	// ---- Building a case from what the dashboard has; only what the asking user may see ----

	struct Built {
		ModeratorPrompt::Case c;
		int64_t itemId{};
		uint32_t accountId{};   // whose account an action would be on (0: unknown)
		LWOOBJID characterId{};
		std::string error;
		eHTTPStatusCode status{ eHTTPStatusCode::BAD_REQUEST };
	};

	std::string CharacterName(LWOOBJID id) {
		if (!id) return "";
		const auto info = Database::Get()->GetCharacterInfo(id);
		return info ? info->name : "";
	}

	std::string ZoneName(uint32_t zoneId) {
		const auto& zones = ZoneNames();
		const auto key = std::to_string(zoneId);
		return zones.contains(key) && zones[key].is_string() ? zones[key].get<std::string>() : "Zone " + key;
	}

	ModeratorPrompt::ChatLine Line(const IChatLog::ChatMessage& m, uint64_t subjectId, LWOOBJID subjectSender) {
		return { m.time, m.channel, m.senderName, m.recipientName, m.message, m.blocked, subjectId ? m.id == subjectId : m.senderId == subjectSender };
	}

	// Chat by or to these characters from `window` seconds before `center` to as long after, the lines closest to it
	std::vector<IChatLog::ChatMessage> ChatAround(const HTTPContext& context, const std::vector<LWOOBJID>& characters, int64_t center, int64_t window) {
		std::map<uint64_t, IChatLog::ChatMessage> found;
		for (const auto character : characters) {
			if (!character) continue;
			IChatLog::ChatQuery query;
			query.characterId = character;
			query.since = center - window;
			query.includePrivate = Can(context, "chat_private");
		query.includeWhispers = Can(context, "chat_dms");
			query.includeWhispers = Can(context, "chat_dms");
			query.limit = 200;
			for (auto& m : Database::Get()->GetChatMessages(query)) {
				if (m.time <= center + window) found[m.id] = std::move(m);
			}
		}
		std::vector<IChatLog::ChatMessage> lines;
		for (auto& [id, m] : found) lines.push_back(std::move(m));
		if (lines.size() > MAX_CHAT_LINES) {
			std::stable_sort(lines.begin(), lines.end(), [center](const auto& a, const auto& b) { return std::llabs(a.time - center) < std::llabs(b.time - center); });
			lines.resize(MAX_CHAT_LINES);
		}
		std::sort(lines.begin(), lines.end(), [](const auto& a, const auto& b) { return a.time != b.time ? a.time < b.time : a.id < b.id; });
		return lines;
	}

	void AddAccountHistory(const HTTPContext& context, Built& built, LWOOBJID nameDecisionsFor, uint64_t exceptReport) {
		const auto accountId = built.accountId;
		if (accountId) {
			const auto since = Strikes::CountsSince();
			built.c.activeStrikes = Strikes::Active(accountId);
			for (const auto& strike : Database::Get()->GetStrikes(accountId)) {
				if (built.c.strikes.size() >= MAX_STRIKES) break;
				// Staff names stay out
				built.c.strikes.push_back({ strike.createdAt, strike.source, strike.subject, strike.reason, strike.revokedAt == 0 && strike.createdAt >= since });
			}
			if (Can(context, "accounts_notes")) {
				for (const auto& note : Database::Get()->GetAccountNotes(accountId)) {
					if (built.c.history.size() >= MAX_HISTORY) break;
					built.c.history.push_back({ note.createdAt, note.kind, note.text });
				}
			}
			if (Can(context, "player_reports_view")) {
				uint32_t added = 0;
				for (const auto& report : Database::Get()->GetPlayerReports({ .status = -1, .accountId = accountId, .limit = MAX_EARLIER_REPORTS + 1 })) {
					if (report.id == exceptReport || added >= MAX_EARLIER_REPORTS) continue;
					const auto status = static_cast<ePlayerReportStatus>(report.status);
					built.c.history.push_back({ report.createdAt, "earlier report about them (" + GameLabels::Name(status) + ")",
						ModeratorPrompt::Clip(report.body, 200) + (report.resolution.empty() ? "" : " -> staff: " + report.resolution) });
					added++;
				}
			}
		}
		if (nameDecisionsFor) {
			for (const auto& d : Database::Get()->GetModerationDecisions("name", nameDecisionsFor, 5)) {
				built.c.history.push_back({ d.value("time", int64_t{}), d.value("approved", false) ? "name approved" : "name rejected",
					d.value("subject", "") + (d.value("reason", "").empty() ? "" : " (" + d.value("reason", "") + ")") });
			}
		}
		std::stable_sort(built.c.history.begin(), built.c.history.end(), [](const auto& a, const auto& b) { return a.time > b.time; });
	}

	Built Fail(eHTTPStatusCode status, std::string error) {
		Built built;
		built.status = status;
		built.error = std::move(error);
		return built;
	}

	Built BuildReport(const HTTPContext& context, uint64_t id, const Settings& settings) {
		if (!Can(context, "player_reports_view")) return Fail(eHTTPStatusCode::FORBIDDEN, "Reading player reports needs player_reports_view");
		const auto report = Database::Get()->GetPlayerReport(id);
		if (!report) return Fail(eHTTPStatusCode::NOT_FOUND, "No such report");
		Built built;
		built.c.kind = eKind::PLAYER_REPORT;
		built.itemId = static_cast<int64_t>(report->id);
		built.accountId = report->targetAccountId;
		built.characterId = report->targetCharacterId;
		const auto kind = magic_enum::enum_cast<ePlayerReportKind>(report->kind);
		auto& item = built.c.item;
		item["what_was_reported"] = kind ? GameLabels::Name(*kind) : report->kind;
		item["sent"] = ModeratorPrompt::FormatTime(report->createdAt);
		item["reporter"] = CharacterName(report->reporterId);
		item["reported_player"] = report->targetCharacterId ? CharacterName(report->targetCharacterId) : "unknown";
		if (report->zoneId) item["where"] = ZoneName(report->zoneId);
		if (report->objectLot && (!kind || *kind != ePlayerReportKind::PLAYER)) item["model"] = ClientAssets::ItemName(report->objectLot);
		if (report->propertyId) {
			if (const auto property = Database::Get()->GetPropertyInfo(report->propertyId)) item["property"] = property->name;
		}
		item["report_text"] = report->body;
		item["status"] = GameLabels::Name(static_cast<ePlayerReportStatus>(report->status));

		if (Can(context, "chat_view")) {
			const auto window = static_cast<int64_t>(settings.chatMinutes) * 60;
			for (const auto& m : ChatAround(context, { report->targetCharacterId, report->reporterId }, report->createdAt, window)) {
				auto line = Line(m, 0, report->targetCharacterId);
				built.c.chat.push_back(std::move(line));
			}
			built.c.chatNote = "Chat of the reported player and the reporter from " + std::to_string(settings.chatMinutes) + " minutes before the report to as long after; "
				"being_judged marks the reported player's own lines.";
		} else {
			built.c.chatNote = "No chat included: the staff member asking can't read the chat log.";
		}
		AddAccountHistory(context, built, 0, report->id);
		return built;
	}

	std::optional<IChatLog::ChatMessage> FindMessage(const HTTPContext& context, uint64_t id) {
		IChatLog::ChatQuery query;
		query.afterId = id - 1;
		query.limit = 1;
		query.includePrivate = Can(context, "chat_private");
		query.includeWhispers = Can(context, "chat_dms");
		const auto found = Database::Get()->GetChatMessages(query);
		if (found.empty() || found.front().id != id) return std::nullopt;
		return found.front();
	}

	Built BuildChatMessage(const HTTPContext& context, uint64_t id, const Settings& settings) {
		if (!Can(context, "chat_view")) return Fail(eHTTPStatusCode::FORBIDDEN, "Reading chat needs chat_view");
		const auto message = id ? FindMessage(context, id) : std::nullopt;
		if (!message) return Fail(eHTTPStatusCode::NOT_FOUND, "No such message (or it's private chat you can't read)");
		if (!message->senderId || message->channel == "web") return Fail(eHTTPStatusCode::BAD_REQUEST, "That message came from the web or a bot, not a player");
		Built built;
		built.c.kind = eKind::CHAT_MESSAGE;
		built.itemId = static_cast<int64_t>(message->id);
		built.accountId = message->accountId;
		built.characterId = message->senderId;
		auto& item = built.c.item;
		item["sender"] = message->senderName;
		item["channel"] = message->channel;
		if (!message->recipientName.empty()) item["to"] = message->recipientName;
		if (message->zoneId) item["where"] = ZoneName(message->zoneId);
		item["when"] = ModeratorPrompt::FormatTime(message->time);
		item["message"] = message->message;
		item["stopped_by_filter"] = message->blocked;
		const auto window = static_cast<int64_t>(settings.chatMinutes) * 60;
		for (const auto& m : ChatAround(context, { message->senderId }, message->time, window)) built.c.chat.push_back(Line(m, message->id, 0));
		built.c.chatNote = "The sender's chat from " + std::to_string(settings.chatMinutes) + " minutes before the message to as long after.";
		AddAccountHistory(context, built, 0, 0);
		return built;
	}

	Built BuildPlayerChat(const HTTPContext& context, LWOOBJID characterId) {
		if (!Can(context, "chat_view")) return Fail(eHTTPStatusCode::FORBIDDEN, "Reading chat needs chat_view");
		const auto info = characterId ? Database::Get()->GetCharacterInfo(characterId) : std::nullopt;
		if (!info) return Fail(eHTTPStatusCode::NOT_FOUND, "No such character");
		IChatLog::ChatQuery query;
		query.characterId = characterId;
		query.includePrivate = Can(context, "chat_private");
		query.includeWhispers = Can(context, "chat_dms");
		query.newestFirst = true;
		query.limit = MAX_CHAT_LINES;
		auto messages = Database::Get()->GetChatMessages(query);
		if (messages.empty()) return Fail(eHTTPStatusCode::BAD_REQUEST, info->name + " has no chat in the log");
		std::reverse(messages.begin(), messages.end());
		Built built;
		built.c.kind = eKind::PLAYER_CHAT;
		built.itemId = characterId;
		built.accountId = info->accountId;
		built.characterId = characterId;
		built.c.item["player"] = info->name;
		built.c.item["from"] = ModeratorPrompt::FormatTime(messages.front().time);
		built.c.item["to"] = ModeratorPrompt::FormatTime(messages.back().time);
		for (const auto& m : messages) built.c.chat.push_back(Line(m, 0, characterId));
		built.c.chatNote = "Their latest " + std::to_string(messages.size()) + " chat lines; being_judged marks their own.";
		AddAccountHistory(context, built, 0, 0);
		return built;
	}

	Built BuildName(const HTTPContext& context, LWOOBJID characterId) {
		if (!Can(context, "moderate_names")) return Fail(eHTTPStatusCode::FORBIDDEN, "Moderating names needs moderate_names");
		const auto info = characterId ? Database::Get()->GetCharacterInfo(characterId) : std::nullopt;
		if (!info) return Fail(eHTTPStatusCode::NOT_FOUND, "No such character");
		if (info->pendingName.empty()) return Fail(eHTTPStatusCode::BAD_REQUEST, info->name + " has no name waiting for approval");
		Built built;
		built.c.kind = eKind::NAME;
		built.itemId = characterId;
		built.accountId = info->accountId;
		built.characterId = characterId;
		built.c.item["requested_name"] = info->pendingName;
		built.c.item["current_name"] = info->name;
		AddAccountHistory(context, built, characterId, 0);
		return built;
	}

	Built BuildPetName(const HTTPContext& context, LWOOBJID petId) {
		if (!Can(context, "moderate_pet_names")) return Fail(eHTTPStatusCode::FORBIDDEN, "Moderating pet names needs moderate_pet_names");
		const auto pet = petId ? Database::Get()->GetPetNameInfo(petId) : std::nullopt;
		if (!pet || pet->petName.empty()) return Fail(eHTTPStatusCode::NOT_FOUND, "No such pet name");
		Built built;
		built.c.kind = eKind::PET_NAME;
		built.itemId = petId;
		built.characterId = pet->ownerId;
		built.c.item["pet_name"] = pet->petName;
		if (const auto owner = pet->ownerId ? Database::Get()->GetCharacterInfo(pet->ownerId) : std::nullopt) {
			built.accountId = owner->accountId;
			built.c.item["owner"] = owner->name;
		}
		for (const auto& d : Database::Get()->GetModerationDecisions("pet_name", petId, 5)) {
			built.c.history.push_back({ d.value("time", int64_t{}), d.value("approved", false) ? "pet name approved" : "pet name rejected",
				d.value("subject", "") + (d.value("reason", "").empty() ? "" : " (" + d.value("reason", "") + ")") });
		}
		AddAccountHistory(context, built, 0, 0);
		return built;
	}

	Built BuildEconomyFlag(const HTTPContext& context, uint64_t id) {
		if (!Can(context, "reports_view")) return Fail(eHTTPStatusCode::FORBIDDEN, "Economy reports need reports_view");
		const auto flag = Database::Get()->GetEconomyFlagRow(id);
		if (!flag) return Fail(eHTTPStatusCode::NOT_FOUND, "No such flag");
		Built built;
		built.c.kind = eKind::ECONOMY_FLAG;
		built.itemId = static_cast<int64_t>(flag->id);
		built.characterId = flag->characterId;
		static const std::map<uint8_t, std::string> KINDS{ {1, "unusual coin income in a day"}, {2, "spike in an item being created"}, {3, "an item that exists in more than one place (duplicate)"} };
		auto& item = built.c.item;
		item["check"] = KINDS.contains(flag->kind) ? KINDS.at(flag->kind) : "unknown";
		if (flag->day) item["day"] = ModeratorPrompt::FormatTime(static_cast<int64_t>(flag->day) * 86400).substr(0, 10);
		if (flag->characterId) {
			if (const auto owner = Database::Get()->GetCharacterInfo(flag->characterId)) {
				built.accountId = owner->accountId;
				item["character"] = owner->name;
			}
		}
		if (flag->lot) item["item"] = ClientAssets::ItemName(flag->lot);
		item["value"] = flag->value;
		item["usual_value"] = flag->baseline;
		item["details"] = flag->details;
		if (flag->characterId) {
			for (const auto& earlier : Database::Get()->GetEconomyFlagsFor(flag->characterId, 10)) {
				if (earlier.value("id", int64_t{}) == static_cast<int64_t>(flag->id)) continue;
				static const std::map<int, std::string> STATUS{ {0, "open"}, {1, "dismissed"}, {2, "actioned"} };
				const auto status = earlier.value("status", 0);
				built.c.history.push_back({ earlier.value("created_at", int64_t{}), "earlier economy flag (" + (STATUS.contains(status) ? STATUS.at(status) : "?") + ")",
					earlier.value("details", "") });
			}
		}
		AddAccountHistory(context, built, 0, 0);
		return built;
	}

	Built BuildCase(const HTTPContext& context, eKind kind, const nlohmann::json& body, const Settings& settings) {
		const auto& raw = body.contains("id") ? body["id"] : nlohmann::json();
		const auto text = raw.is_string() ? raw.get<std::string>() : raw.is_number_integer() ? raw.dump() : "";
		const auto id = GeneralUtils::TryParse<uint64_t>(text);
		if (kind == eKind::PLAYER_CHAT) {
			// A character typed on the chat log: an ID or a name
			const auto character = ResolveCharacter(text);
			if (!character) return Fail(eHTTPStatusCode::NOT_FOUND, "No such character");
			return BuildPlayerChat(context, *character);
		}
		if (!id || *id == 0) return Fail(eHTTPStatusCode::BAD_REQUEST, "Which one? (id)");
		switch (kind) {
		case eKind::PLAYER_REPORT: return BuildReport(context, *id, settings);
		case eKind::CHAT_MESSAGE: return BuildChatMessage(context, *id, settings);
		case eKind::NAME: return BuildName(context, static_cast<LWOOBJID>(*id));
		case eKind::PET_NAME: return BuildPetName(context, static_cast<LWOOBJID>(*id));
		case eKind::ECONOMY_FLAG: return BuildEconomyFlag(context, *id);
		default: return Fail(eHTTPStatusCode::BAD_REQUEST, "Unknown kind");
		}
	}

	std::string Summary(const ModeratorPrompt::Suggestion& s) {
		std::string text = s.action;
		if (s.days) text += " " + std::to_string(s.days) + " day(s)";
		if (s.strike && s.action != "strike") text += " + strike";
		return text + ", " + s.confidence + " confidence";
	}

	void Finish(Job& job) {
		const auto key = ModeratorPrompt::KindKey(job.kind);
		g_InFlight.erase(key + ":" + std::to_string(job.itemId));

		IAiSuggestions::AiSuggestion row;
		row.kind = key;
		row.itemId = job.itemId;
		row.fingerprint = job.fingerprint;
		row.requestedById = job.context.accountId;
		row.requestedBy = job.context.authenticatedUser;
		row.createdAt = Now();
		row.model = job.result.model.empty() ? job.config.model : job.result.model;
		row.inputTokens = job.result.inputTokens;
		row.outputTokens = job.result.outputTokens;
		row.context = ModeratorPrompt::SafeJson(job.caseJson);

		std::string message;
		bool success = false;
		if (!job.result.ok) {
			row.status = IAiSuggestions::eAiStatus::FAILED;
			row.error = job.result.message;
			message = job.result.message;
		} else if (!job.parsed.suggestion) {
			row.status = IAiSuggestions::eAiStatus::REJECTED;
			row.error = job.parsed.error;
			message = "The suggestion was thrown away: " + job.parsed.error;
		} else {
			row.status = IAiSuggestions::eAiStatus::OK;
			row.suggestion = job.parsed.suggestion->ToJson().dump();
			message = "Suggestion ready";
			success = true;
		}
		try {
			row.id = Database::Get()->InsertAiSuggestion(row);
		} catch (const std::exception& ex) {
			LOG("Could not store an AI suggestion: %s", ex.what());
		}

		const std::string subject = "AI suggestion for " + ModeratorPrompt::KindLabel(job.kind) + " #" + std::to_string(job.itemId) + " (" + row.model + ", " +
			std::to_string(row.inputTokens) + "+" + std::to_string(row.outputTokens) + " tokens): ";
		Audit(job.context, "ai_suggest", subject + (success ? Summary(*job.parsed.suggestion) : "failed: " + ModeratorPrompt::Clip(message, 200)),
			job.accountId ? AuditTarget::Account(job.accountId) : AuditTarget{});
		if (!job.result.ok) LOG("AI moderator helper request failed: %s", job.result.message.c_str());

		PlayerActions::Outcome outcome{ success, message };
		if (success) {
			outcome.data = RowJson(row, false, job.accountId, job.characterId);
			outcome.data["budget"] = BudgetJson(ReadSettings());
		}
		PlayerActions::Finish(job.requestId, outcome);
	}
}

namespace ModeratorHelper {
	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/ai/status", Perm("ai_suggest"),
			"Whether the AI moderator helper is on and set up, and today's budget: {enabled, configured, model, budget: {used_today, per_day, remaining_today, per_minute, resets_at}, tokens_today}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto settings = ReadSettings();
				const auto usage = Database::Get()->GetAiUsageSince(AiBudget::Day(Now()) * 86400);
				std::string error;
				const bool baseOk = ClaudeClient::Endpoint(settings.client.base, error).has_value();
				JsonSuccess(reply, { {"enabled", settings.enabled}, {"configured", !settings.client.apiKey.empty() && baseOk}, {"model", settings.client.model},
					{"budget", BudgetJson(settings)}, {"tokens_today", { {"input", usage.inputTokens}, {"output", usage.outputTokens} }},
					{"problem", !baseOk ? error : settings.client.apiKey.empty() ? "No Claude API key is set (claude_api_key)" : ""} });
			});

		Route(eHTTPMethod::POST, "/api/ai/suggest", Perm("ai_suggest"),
			"Ask the AI moderator helper to draft a suggestion for staff. Body: {kind (player_report, chat_message, player_chat, name, pet_name, economy_flag), "
			"id (the report, message, character (ID or name for player_chat), pet or flag), refresh (true: ask again even if a stored suggestion fits)}. "
			"Also needs the permission to see that item. Returns {cached: true, suggestion...} when a stored one fits, else {requestId} for the result. "
			"Nothing is applied: staff decide",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto kind = ModeratorPrompt::ParseKind(body->value("kind", ""));
				if (!kind) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown kind");

				const auto settings = ReadSettings();
				if (!settings.enabled) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The AI moderator helper is off (Settings, Dashboard, AI moderator helper)");
				if (settings.client.apiKey.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No Claude API key is set (claude_api_key in Settings, Dashboard, AI moderator helper)");
				std::string baseError;
				if (!ClaudeClient::Endpoint(settings.client.base, baseError)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, baseError);

				auto built = BuildCase(context, *kind, *body, settings);
				if (!built.error.empty()) return JsonError(reply, built.status, built.error);

				const auto key = ModeratorPrompt::KindKey(*kind);
				const auto fingerprint = ModeratorPrompt::Fingerprint(built.c, settings.rules, settings.client.model);
				const bool refresh = body->value("refresh", false);
				if (!refresh) {
					if (const auto stored = Database::Get()->FindAiSuggestion(key, built.itemId, fingerprint)) {
						auto result = RowJson(*stored, true, built.accountId, built.characterId);
						result["budget"] = BudgetJson(settings);
						return JsonSuccess(reply, result);
					}
				}

				const auto flightKey = key + ":" + std::to_string(built.itemId);
				if (g_InFlight.contains(flightKey)) return JsonError(reply, eHTTPStatusCode::CONFLICT, "A suggestion for this is already on its way; wait a moment");

				const auto now = Now();
				switch (Budget().TryAcquire(settings.limits, now)) {
				case AiBudget::eDecision::DAY:
					return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Today's AI helper budget (" + std::to_string(settings.limits.perDay) +
						" requests) is used up; it resets at midnight UTC (ai_helper_per_day)");
				case AiBudget::eDecision::MINUTE:
					return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many suggestions this minute; try again in " +
						std::to_string(Budget().MinuteWait(settings.limits, now)) + " seconds (ai_helper_per_minute)");
				case AiBudget::eDecision::OK: break;
				}

				Job job;
				job.kind = *kind;
				job.config = settings.client;
				job.caseJson = ModeratorPrompt::CaseJson(built.c);
				job.prompt = ModeratorPrompt::Build(built.c, settings.rules, ModeratorPrompt::RandomToken(), "CANARY-" + ModeratorPrompt::RandomToken(8));
				job.context = context;
				job.context.body.clear();
				job.itemId = built.itemId;
				job.fingerprint = fingerprint;
				job.accountId = built.accountId;
				job.characterId = built.characterId;
				// Every try may take the whole timeout, plus the waits between them
				const auto timeout = std::chrono::seconds(settings.client.timeoutSeconds * ClaudeClient::MAX_ATTEMPTS + 90);
				job.requestId = PlayerActions::Begin(context.accountId, timeout);
				g_InFlight[flightKey] = job.requestId;
				const auto requestId = job.requestId;
				Enqueue(std::move(job));
				JsonSuccess(reply, { {"requestId", requestId}, {"budget", BudgetJson(settings)} });
			});

		Route(eHTTPMethod::GET, "/api/ai/suggestions", Perm("ai_suggest"),
			"Earlier suggestions for an item, newest first. Query: kind, id. Returns {suggestions: [{id, created_at, requested_by, model, input_tokens, output_tokens, status, suggestion, error}]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto kind = ModeratorPrompt::ParseKind(QueryValue(context.queryString, "kind"));
				const auto id = GeneralUtils::TryParse<int64_t>(QueryValue(context.queryString, "id"));
				if (!kind || !id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind and id are needed");
				// The same permission as seeing the item
				static const std::map<eKind, std::string> NEEDS{ {eKind::PLAYER_REPORT, "player_reports_view"}, {eKind::CHAT_MESSAGE, "chat_view"},
					{eKind::PLAYER_CHAT, "chat_view"}, {eKind::NAME, "moderate_names"}, {eKind::PET_NAME, "moderate_pet_names"}, {eKind::ECONOMY_FLAG, "reports_view"} };
				if (!Can(context, NEEDS.at(*kind))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You can't see that item");
				nlohmann::json rows = nlohmann::json::array();
				for (const auto& row : Database::Get()->GetAiSuggestions(ModeratorPrompt::KindKey(*kind), *id, 10)) {
					rows.push_back({ {"id", row.id}, {"created_at", row.createdAt}, {"requested_by", row.requestedBy}, {"model", row.model},
						{"input_tokens", row.inputTokens}, {"output_tokens", row.outputTokens}, {"status", static_cast<int>(row.status)},
						{"suggestion", nlohmann::json::parse(row.suggestion.empty() ? "null" : row.suggestion, nullptr, false)}, {"error", row.error} });
				}
				JsonSuccess(reply, { {"suggestions", rows} });
			});
	}

	void Update() {
		std::deque<Job> done;
		{
			std::lock_guard lock(g_Mutex);
			if (g_Done.empty()) return;
			done.swap(g_Done);
		}
		for (auto& job : done) {
			try {
				Finish(job);
			} catch (const std::exception& ex) {
				LOG("AI moderator helper completion failed: %s", ex.what());
				g_InFlight.erase(ModeratorPrompt::KindKey(job.kind) + ":" + std::to_string(job.itemId));
				PlayerActions::Finish(job.requestId, { false, "The helper failed" });
			}
		}
	}

	void Shutdown() {
		if (!g_Worker.joinable()) return;
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Wake.notify_all();
		// A request in progress finishes first (at most its timeout and retries)
		g_Worker.join();
		g_Jobs.clear();
		g_Done.clear();
	}
}
