#include "Inspector.h"

#include <chrono>
#include <ctime>
#include <map>

#include "Background.h"
#include "BitStreamUtils.h"
#include "Database.h"
#include "DashboardRoutes.h"
#include "Game.h"
#include "InspectorFormat.h"
#include "Logger.h"
#include "MessageCapture.h"
#include "MessageType/Master.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "Scheduler.h"
#include "ServiceType.h"
#include "Web.h"
#include "dConfig.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"

using namespace RouteUtils;

namespace {
	using Clock = std::chrono::steady_clock;
	using Session = IMessageCaptures::MessageCaptureSession;
	using Record = IMessageCaptures::MessageCaptureRecord;

	constexpr const char* TOPIC = "message_capture";
	constexpr const char* PERMISSION = "dev_message_inspector";
	constexpr uint32_t DEFAULT_SECONDS = 120;
	constexpr size_t MAX_RUNNING = 4;
	// Messages saved per capture; a capture stops when it reaches this
	constexpr uint64_t MAX_SAVED = 100000;
	// Finished captures stay in the live list this long, and at most this many are kept there (all are saved)
	constexpr auto KEEP_ENDED = std::chrono::hours(1);
	constexpr size_t MAX_CAPTURES = 20;
	constexpr auto PUSH_INTERVAL = std::chrono::milliseconds(250);
	constexpr auto SAVE_INTERVAL = std::chrono::seconds(1);
	// Ask the worlds again this often while no world has the player (logged out, or changing zones)
	constexpr auto RETRY_INTERVAL = std::chrono::seconds(3);
	// The world sends something at least every few seconds while it captures; silence this long means it went away
	constexpr auto SILENT_AFTER = std::chrono::seconds(15);
	// Messages per request for a saved session
	constexpr uint32_t PAGE_DEFAULT = 2000;
	constexpr uint32_t PAGE_MAX = 5000;
	constexpr uint32_t SESSIONS_MAX = 200;
	// Opening a finished capture is audited at most this often per staff member and capture
	constexpr auto VIEW_AUDIT_INTERVAL = std::chrono::hours(1);
	constexpr int64_t DEFAULT_KEEP_DAYS = 30;
	constexpr int64_t DEFAULT_MAX_MB = 1024;

	enum class eState : uint8_t { WAITING, CAPTURING, ENDED };

	struct Capture {
		Session session;                                  // saved; session.id is the capture's id
		Clock::time_point until;
		MessageCaptureControl control;
		eState state{ eState::WAITING };
		Clock::time_point endedAt{};
		Clock::time_point lastHeard{};
		Clock::time_point nextRetry{};
		InspectorFormat::GapCounter gaps;
		std::vector<Record> unsaved;                      // captured since the last save
		nlohmann::json unsent = nlohmann::json::array(); // captured since the last push
		bool changed{};                                   // state changed since the last push
		bool dirty{};                                     // session changed since the last save
	};

	std::map<uint32_t, Capture> g_Captures;
	Clock::time_point g_NextPush{};
	Clock::time_point g_NextSave{};
	std::map<std::pair<uint32_t, uint64_t>, Clock::time_point> g_ViewAudits; // (account, session) -> last audited

	bool Connected() { return Game::server && Game::server->GetIsConnectedToMaster(); }

	int64_t Setting(const std::string& key, int64_t fallback) {
		if (!Game::config) return fallback;
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	void SendControl(const MessageCaptureControl& control) {
		if (!Connected()) return;
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::MESSAGE_CAPTURE_CONTROL);
		control.Serialize(bitStream);
		Game::server->SendToMaster(bitStream);
	}

	// (Re)start in whichever world has the player, for the time that is left
	void SendStart(Capture& capture) {
		const auto left = std::chrono::duration_cast<std::chrono::seconds>(capture.until - Clock::now()).count();
		if (left <= 0) return;
		capture.control.action = eMessageCaptureControl::START;
		capture.control.seconds = static_cast<uint32_t>(left);
		SendControl(capture.control);
		capture.nextRetry = Clock::now() + RETRY_INTERVAL;
	}

	void SendStop(const MessageCaptureControl& from) {
		auto control = from;
		control.action = eMessageCaptureControl::STOP;
		SendControl(control);
	}

	const char* StateName(eState state) {
		switch (state) {
		case eState::WAITING: return "waiting";
		case eState::CAPTURING: return "capturing";
		default: return "ended";
		}
	}

	std::string ZoneName(uint32_t zoneId) {
		const auto& zones = ZoneNames();
		const auto zone = std::to_string(zoneId);
		return zoneId == 0 ? "" : zones.contains(zone) ? zones[zone].get<std::string>() : "Zone " + zone;
	}

	// A session for the page; `state` is the live state, or "ended" for one only in the database
	nlohmann::json SessionJson(const Session& session, const char* state) {
		nlohmann::json zones = nlohmann::json::array();
		for (const auto& visit : GeneralUtils::SplitString(session.zones, ' ')) {
			const auto parts = GeneralUtils::SplitString(visit, ':');
			if (parts.size() != 3) continue;
			const auto zone = GeneralUtils::TryParse<uint32_t>(parts[0]).value_or(0);
			zones.push_back({ {"zone", zone}, {"zoneName", ZoneName(zone)}, {"instance", GeneralUtils::TryParse<uint32_t>(parts[1]).value_or(0)},
				{"clone", GeneralUtils::TryParse<uint32_t>(parts[2]).value_or(0)} });
		}
		return {
			{"id", session.id},
			{"characterId", std::to_string(session.characterId)},
			{"characterName", session.characterName},
			{"accountId", session.accountId},
			{"accountName", session.accountName},
			{"startedBy", session.startedBy},
			{"startedAt", session.startedAt},
			{"endsAt", session.endsAt},
			{"endedAt", session.endedAt},
			{"state", state},
			{"endReason", session.endReason},
			{"zone", session.zoneId},
			{"zoneName", ZoneName(session.zoneId)},
			{"instance", session.instanceId},
			{"clone", session.cloneId},
			{"zones", zones},
			{"received", session.messageCount},
			{"bytes", session.byteCount},
			{"dropped", session.dropped},
			{"toServer", session.toServer},
			{"toClient", session.toClient},
			{"only", InspectorFormat::MessageNames(InspectorFormat::ParseIds(session.onlyMessages))},
			{"skip", InspectorFormat::MessageNames(InspectorFormat::ParseIds(session.skipMessages))},
			{"lastSeq", session.messageCount}
		};
	}

	nlohmann::json Summary(const Capture& capture) {
		return SessionJson(capture.session, StateName(capture.state));
	}

	// Write what was captured since the last save, and the session's details, as one transaction
	void Save(Capture& capture) {
		if (capture.unsaved.empty() && !capture.dirty) return;
		try {
			auto* db = Database::Get();
			DatabaseTransaction transaction(*db);
			db->InsertMessageCaptureEntries(capture.unsaved);
			db->UpdateMessageCaptureSession(capture.session);
			transaction.Commit();
		} catch (const std::exception& e) {
			// Rolled back; dropping this batch keeps one bad write from repeating every second
			LOG("Couldn't save %zu captured message(s) of capture %llu: %s", capture.unsaved.size(), capture.session.id, e.what());
		}
		capture.unsaved.clear();
		capture.dirty = false;
	}

	void End(Capture& capture, const std::string& reason) {
		if (capture.state == eState::ENDED) return;
		capture.state = eState::ENDED;
		capture.session.endReason = reason;
		capture.session.endedAt = std::time(nullptr);
		capture.endedAt = Clock::now();
		capture.changed = true;
		capture.dirty = true;
		Save(capture);
	}

	void Push(Capture& capture) {
		if (capture.unsent.empty() && !capture.changed) return;
		nlohmann::json message{ {"capture", Summary(capture)}, {"entries", std::move(capture.unsent)} };
		capture.unsent = nlohmann::json::array();
		capture.changed = false;
		Game::web.SendWSMessage(TOPIC, message);
	}

	// Drop finished captures from the live list past their keep time, and the oldest finished ones over the limit
	void Prune(Clock::time_point now) {
		std::erase_if(g_Captures, [now](const auto& entry) { return entry.second.state == eState::ENDED && now - entry.second.endedAt > KEEP_ENDED; });
		while (g_Captures.size() > MAX_CAPTURES) {
			auto oldest = g_Captures.end();
			for (auto it = g_Captures.begin(); it != g_Captures.end(); ++it) {
				if (it->second.state == eState::ENDED && (oldest == g_Captures.end() || it->second.endedAt < oldest->second.endedAt)) oldest = it;
			}
			if (oldest == g_Captures.end()) break;
			g_Captures.erase(oldest);
		}
		std::erase_if(g_ViewAudits, [now](const auto& entry) { return now - entry.second > VIEW_AUDIT_INTERVAL; });
	}

	// Where the world that sent this is; a new world is added to the session's list
	void NoteWorld(Capture& capture, const MessageCaptureData& data) {
		auto& session = capture.session;
		if (session.zoneId == data.zoneId && session.instanceId == data.instanceId && session.cloneId == data.cloneId && !session.zones.empty()) return;
		session.zoneId = data.zoneId;
		session.instanceId = data.instanceId;
		session.cloneId = data.cloneId;
		const auto visit = std::to_string(data.zoneId) + ":" + std::to_string(data.instanceId) + ":" + std::to_string(data.cloneId);
		const auto last = session.zones.rfind(' ');
		if (session.zones.substr(last == std::string::npos ? 0 : last + 1) != visit) session.zones += (session.zones.empty() ? "" : " ") + visit;
		capture.changed = true;
		capture.dirty = true;
	}

	MessageCaptureControl ControlFor(const Session& session) {
		MessageCaptureControl control;
		control.captureId = static_cast<uint32_t>(session.id);
		control.characterId = session.characterId;
		control.toServer = session.toServer;
		control.toClient = session.toClient;
		control.only = InspectorFormat::ParseIds(session.onlyMessages);
		control.skip = InspectorFormat::ParseIds(session.skipMessages);
		return control;
	}

	// The running capture with this id, if it is still in the live list
	Capture* Live(uint64_t id) {
		const auto it = id <= UINT32_MAX ? g_Captures.find(static_cast<uint32_t>(id)) : g_Captures.end();
		return it == g_Captures.end() ? nullptr : &it->second;
	}

	Capture* FindCapture(const HTTPContext& context, HTTPReply& reply) {
		const auto id = PathId<uint32_t>(context.path, 3);
		auto* capture = id ? Live(*id) : nullptr;
		if (!capture) JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such running capture");
		return capture;
	}

	// The session in the path (saving a running one first, so what is read is complete); writes a 404 if there is none
	std::optional<Session> FindSession(const HTTPContext& context, HTTPReply& reply) {
		const auto id = PathId<uint64_t>(context.path, 3);
		if (id) {
			if (auto* capture = Live(*id)) {
				Save(*capture);
				return capture->session;
			}
			if (auto session = Database::Get()->GetMessageCaptureSession(*id)) return session;
		}
		JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such capture (it may have been deleted)");
		return std::nullopt;
	}

	const char* StateOf(const Session& session) {
		const auto* capture = Live(session.id);
		return capture ? StateName(capture->state) : "ended";
	}

	std::string Describe(const Session& session) {
		return session.characterName + " (capture " + std::to_string(session.id) + " by " + session.startedBy + ", " +
			std::to_string(session.messageCount) + " messages)";
	}

	// Delete saved captures past inspector_session_days, then the oldest over inspector_max_mb
	void PruneSessions(Scheduler::RunPtr run) {
		const auto days = Setting("inspector_session_days", DEFAULT_KEEP_DAYS);
		const auto maxMb = Setting("inspector_max_mb", DEFAULT_MAX_MB);
		run->Log("inspector_session_days = " + (days > 0 ? std::to_string(days) : "0 (no age limit)"));
		run->Log("inspector_max_mb = " + (maxMb > 0 ? std::to_string(maxMb) : "0 (no size limit)"));
		const bool queued = Background::Run("message_capture_pruning", [days, maxMb](GameDatabase& db) -> nlohmann::json {
			std::vector<InspectorFormat::StoredSession> stored;
			uint64_t total = 0;
			for (uint32_t offset = 0;; offset += 1000) {
				const auto page = db.GetMessageCaptureSessions({ .order = IMessageCaptures::eSessionOrder::STARTED, .ascending = true, .limit = 1000, .offset = offset });
				for (const auto& session : page) {
					stored.push_back({ session.id, session.startedAt, session.byteCount, session.endedAt == 0 });
					total += session.byteCount;
				}
				if (page.size() < 1000) break;
			}
			const auto expired = InspectorFormat::SelectExpired(stored, std::time(nullptr), days, maxMb > 0 ? static_cast<uint64_t>(maxMb) * 1024 * 1024 : 0);
			uint64_t freed = 0;
			for (const auto id : expired) {
				const auto it = std::ranges::find(stored, id, &InspectorFormat::StoredSession::id);
				if (it != stored.end()) freed += it->bytes;
				db.DeleteMessageCaptureSession(id);
			}
			return { {"sessions", stored.size()}, {"deleted", expired.size()}, {"total", total}, {"freed", freed} };
		}, [run](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			const auto mb = [](uint64_t bytes) { return std::to_string((bytes + 1024 * 1024 - 1) / (1024 * 1024)) + " MB"; };
			run->Log(std::to_string(result["sessions"].get<uint64_t>()) + " saved capture(s), about " + mb(result["total"].get<uint64_t>()));
			run->Finish(true, "Deleted " + std::to_string(result["deleted"].get<uint64_t>()) + " saved capture(s), about " + mb(result["freed"].get<uint64_t>()));
		});
		if (!queued) run->Finish(false, "Pruning is already running");
	}

	// ?character=&account=&staff=&since=&until=&running=1&order=&dir=asc&offset=&limit=. Sets `error` for a filter it can't read.
	std::optional<IMessageCaptures::SessionQuery> SessionQueryFrom(const std::string& query, std::string& error, bool& none) {
		IMessageCaptures::SessionQuery q;
		const auto character = QueryValue(query, "character");
		if (!character.empty()) {
			// An ID as it is (the character may have been deleted since), a name by looking it up
			const auto number = GeneralUtils::TryParse<LWOOBJID>(character);
			const auto id = number ? number : ResolveCharacter(character);
			if (!id) none = true;
			else q.characterId = *id;
		}
		const auto account = QueryValue(query, "account");
		if (!account.empty()) {
			const auto number = GeneralUtils::TryParse<uint32_t>(account);
			const auto info = number ? std::nullopt : Database::Get()->GetAccountInfo(account);
			if (number) q.accountId = *number;
			else if (info) q.accountId = info->id;
			else none = true;
		}
		q.startedBy = QueryValue(query, "staff");
		q.since = GeneralUtils::TryParse<int64_t>(QueryValue(query, "since")).value_or(0);
		q.until = GeneralUtils::TryParse<int64_t>(QueryValue(query, "until")).value_or(0);
		q.unfinishedOnly = QueryValue(query, "running") == "1";
		const auto order = QueryValue(query, "order");
		if (!order.empty()) {
			const auto parsed = InspectorFormat::ParseOrder(order);
			if (!parsed) {
				error = "Unknown order: " + order;
				return std::nullopt;
			}
			q.order = *parsed;
		}
		q.ascending = QueryValue(query, "dir") == "asc";
		q.offset = GeneralUtils::TryParse<uint32_t>(QueryValue(query, "offset")).value_or(0);
		q.limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(query, "limit")).value_or(25), 1, SESSIONS_MAX);
		return q;
	}
}

namespace Inspector {
	void Initialize() {
		Scheduler::Register({ "message_capture_pruning", "Message capture pruning",
			"Deletes saved game message captures older than inspector_session_days, then the oldest ones while all of them together take more "
			"than inspector_max_mb. Running captures are never deleted.", "50 0 * * *", PruneSessions });

		// Captures that were running when the dashboard stopped: carry on with those that still have time left
		const auto now = static_cast<int64_t>(std::time(nullptr));
		std::vector<Session> unfinished;
		try {
			unfinished = Database::Get()->GetMessageCaptureSessions({ .unfinishedOnly = true, .ascending = true, .limit = 1000 });
		} catch (const std::exception& e) {
			LOG("Couldn't read unfinished message captures: %s", e.what());
		}
		for (auto& session : unfinished) {
			const bool taken = std::ranges::any_of(g_Captures, [&](const auto& entry) { return entry.second.session.characterId == session.characterId; });
			if (session.endsAt > now && session.id <= UINT32_MAX && g_Captures.size() < MAX_RUNNING && !taken) {
				Capture capture;
				capture.session = session;
				capture.control = ControlFor(session);
				capture.until = Clock::now() + std::chrono::seconds(session.endsAt - now);
				capture.nextRetry = Clock::now();
				capture.lastHeard = Clock::now();
				LOG("Carrying on message capture %llu of %s", session.id, session.characterName.c_str());
				g_Captures.emplace(static_cast<uint32_t>(session.id), std::move(capture));
				continue;
			}
			session.endedAt = std::min(now, session.endsAt);
			session.endReason = "The dashboard stopped while it ran";
			try {
				Database::Get()->UpdateMessageCaptureSession(session);
			} catch (const std::exception& e) {
				LOG("Couldn't end message capture %llu: %s", session.id, e.what());
			}
		}
	}

	void HandleData(const MessageCaptureData& data) {
		const auto it = g_Captures.find(data.captureId);
		if (it == g_Captures.end() || it->second.state == eState::ENDED) {
			// A world still sending for a capture that ended here (it missed the stop, or the dashboard restarted): stop it again
			if (data.status != eMessageCaptureStatus::ENDED) {
				MessageCaptureControl control;
				control.captureId = data.captureId;
				control.characterId = data.characterId;
				SendStop(control);
			}
			return;
		}
		auto& capture = it->second;

		capture.lastHeard = Clock::now();
		NoteWorld(capture, data);
		// The world numbers its messages from 1 again when the capture starts there
		if (data.status == eMessageCaptureStatus::STARTED) capture.gaps.Restart();
		if (capture.state == eState::WAITING && data.status != eMessageCaptureStatus::ENDED) {
			capture.state = eState::CAPTURING;
			capture.changed = true;
		}
		auto& session = capture.session;
		session.dropped += data.dropped;
		if (data.dropped) capture.changed = capture.dirty = true;
		bool full = false;
		for (const auto& entry : data.entries) {
			if (session.messageCount >= MAX_SAVED) {
				full = true;
				break;
			}
			const auto gap = capture.gaps.Next(entry.sequence);
			auto record = InspectorFormat::ToRecord(entry, session.id, static_cast<uint32_t>(++session.messageCount), gap, data.zoneId, data.instanceId, data.cloneId);
			session.byteCount += InspectorFormat::StoredBytes(record);
			capture.unsent.push_back(InspectorFormat::RecordJson(record));
			capture.unsaved.push_back(std::move(record));
			capture.dirty = true;
		}
		if (full) {
			SendStop(capture.control);
			End(capture, "Reached " + std::to_string(MAX_SAVED) + " messages, the most one capture keeps");
			return;
		}

		if (data.status == eMessageCaptureStatus::ENDED) {
			switch (data.reason) {
			case eMessageCaptureEnd::PLAYER_LEFT:
				// Most often a zone change: look for the player in the other worlds until the time runs out
				capture.state = eState::WAITING;
				capture.nextRetry = Clock::now() + RETRY_INTERVAL;
				capture.changed = true;
				break;
			case eMessageCaptureEnd::TIME_LIMIT:
				End(capture, "Time limit reached");
				break;
			default:
				End(capture, "Stopped");
				break;
			}
		}
	}

	void Update() {
		const auto now = Clock::now();
		if (g_Captures.empty()) return;

		if (now >= g_NextSave) {
			g_NextSave = now + SAVE_INTERVAL;
			for (auto& [id, capture] : g_Captures) Save(capture);
		}

		if (now < g_NextPush) return;
		g_NextPush = now + PUSH_INTERVAL;
		for (auto& [id, capture] : g_Captures) {
			if (capture.state != eState::ENDED) {
				if (now >= capture.until) {
					SendStop(capture.control);
					End(capture, "Time limit reached");
				} else if (capture.state == eState::CAPTURING && now - capture.lastHeard > SILENT_AFTER) {
					// The world stopped answering (crashed or shut down); look for the player again
					capture.state = eState::WAITING;
					capture.changed = true;
					SendStart(capture);
				} else if (capture.state == eState::WAITING && now >= capture.nextRetry) {
					SendStart(capture);
				}
			}
			Push(capture);
		}
		Prune(now);
	}

	void Shutdown() {
		for (auto& [id, capture] : g_Captures) Save(capture);
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription(TOPIC, std::function<uint8_t()>([] { return Permissions::Level(PERMISSION); }));

		Route(eHTTPMethod::GET, "/inspector", Perm(PERMISSION), "The game message inspector",
			[](HTTPReply& reply, const HTTPContext& context) {
				RenderPage(reply, context, "inspector.jinja2", "inspector");
			});

		Route(eHTTPMethod::GET, "/api/inspector/messages", Perm(PERMISSION), "Every game message the server knows, by ID and name, for capture filters",
			[](HTTPReply& reply, const HTTPContext&) {
				static const auto messages = InspectorFormat::AllMessages();
				JsonSuccess(reply, { {"messages", messages} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/captures", Perm(PERMISSION), "Running and recently finished message captures",
			[](HTTPReply& reply, const HTTPContext&) {
				nlohmann::json captures = nlohmann::json::array();
				for (auto it = g_Captures.rbegin(); it != g_Captures.rend(); ++it) captures.push_back(Summary(it->second));
				JsonSuccess(reply, { {"captures", captures}, {"maxSeconds", MessageCapture::MAX_SECONDS}, {"defaultSeconds", DEFAULT_SECONDS} });
			});

		Route(eHTTPMethod::POST, "/api/inspector/captures", Perm(PERMISSION),
			"Start capturing an online player's game messages. Body: {character (name or ID), seconds (1-900), toServer, toClient, only: [message names or IDs], skip: [...]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto characterId = ResolveCharacter(body->value("character", std::string{}));
				const auto info = characterId ? Database::Get()->GetCharacterInfo(*characterId) : std::nullopt;
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such character");

				const auto seconds = body->value("seconds", DEFAULT_SECONDS);
				if (seconds < 1 || seconds > MessageCapture::MAX_SECONDS) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A capture can run from 1 second to " + std::to_string(MessageCapture::MAX_SECONDS / 60) + " minutes");
				}
				MessageCaptureControl control;
				control.characterId = info->id;
				control.toServer = body->value("toServer", true);
				control.toClient = body->value("toClient", true);
				if (!control.toServer && !control.toClient) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Capture at least one direction");
				std::string error;
				const auto only = InspectorFormat::ParseMessageList(body->contains("only") ? (*body)["only"] : nlohmann::json(), error);
				const auto skip = only ? InspectorFormat::ParseMessageList(body->contains("skip") ? (*body)["skip"] : nlohmann::json(), error) : std::nullopt;
				if (!only || !skip) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				control.only = *only;
				control.skip = *skip;

				size_t running = 0;
				for (const auto& [id, capture] : g_Captures) {
					if (capture.state == eState::ENDED) continue;
					running++;
					if (capture.session.characterId == info->id) {
						return JsonError(reply, eHTTPStatusCode::CONFLICT, info->name + " is already being captured (capture " + std::to_string(id) + ")");
					}
				}
				if (running >= MAX_RUNNING) return JsonError(reply, eHTTPStatusCode::CONFLICT, "At most " + std::to_string(MAX_RUNNING) + " captures can run at once; stop one first");
				if (!Connected()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");

				Capture capture;
				auto& session = capture.session;
				session.characterId = info->id;
				session.characterName = info->name;
				session.accountId = info->accountId;
				const auto account = Database::Get()->GetAccountById(info->accountId);
				session.accountName = account.value("name", std::string{});
				session.startedById = context.accountId;
				session.startedBy = context.authenticatedUser;
				session.startedAt = std::time(nullptr);
				session.endsAt = session.startedAt + seconds;
				session.toServer = control.toServer;
				session.toClient = control.toClient;
				session.onlyMessages = InspectorFormat::IdsText(control.only);
				session.skipMessages = InspectorFormat::IdsText(control.skip);
				session.id = Database::Get()->InsertMessageCaptureSession(session);
				if (session.id == 0 || session.id > UINT32_MAX) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't save the capture");

				capture.until = Clock::now() + std::chrono::seconds(seconds);
				control.captureId = static_cast<uint32_t>(session.id);
				capture.control = control;
				SendStart(capture);
				const auto summary = Summary(capture);
				g_Captures.emplace(control.captureId, std::move(capture));
				Prune(Clock::now());

				Audit(context, "start_message_capture", "Capturing game messages of " + info->name + " for " + std::to_string(seconds) + " s (capture " +
					std::to_string(control.captureId) + ")", AuditTarget::Character(info->id));
				JsonSuccess(reply, { {"capture", summary} });
			});

		Route(eHTTPMethod::POST, "/api/inspector/captures/:id/stop", Perm(PERMISSION), "Stop a running capture",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto* capture = FindCapture(context, reply);
				if (!capture) return;
				if (capture->state == eState::ENDED) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That capture has already ended");
				SendStop(capture->control);
				End(*capture, "Stopped by " + context.authenticatedUser);
				Push(*capture);
				Audit(context, "stop_message_capture", "Stopped capturing game messages of " + capture->session.characterName + " (capture " +
					std::to_string(capture->session.id) + ")", AuditTarget::Character(capture->session.characterId));
				JsonSuccess(reply, { {"capture", Summary(*capture)} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions", Perm(PERMISSION),
			"Saved captures, newest first. Query: ?character=<name or ID>&account=<name or ID>&staff=<who started it>&since=&until=<Unix seconds>"
			"&running=1&order=started|character|started_by|messages|bytes&dir=asc|desc&offset=&limit=<1-200>",
			[](HTTPReply& reply, const HTTPContext& context) {
				std::string error;
				bool none = false;
				const auto query = SessionQueryFrom(context.queryString, error, none);
				if (!query) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				nlohmann::json sessions = nlohmann::json::array();
				uint64_t total = 0;
				// A character or account that doesn't exist matches nothing
				if (!none) {
					total = Database::Get()->CountMessageCaptureSessions(*query);
					for (const auto& session : Database::Get()->GetMessageCaptureSessions(*query)) {
						// A running capture's counts in the database can be a second behind
						const auto* capture = Live(session.id);
						sessions.push_back(capture ? Summary(*capture) : SessionJson(session, "ended"));
					}
				}
				JsonSuccess(reply, { {"sessions", sessions}, {"total", total}, {"offset", query->offset}, {"limit", query->limit},
					{"orders", InspectorFormat::OrderNames()} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id", Perm(PERMISSION), "One saved or running capture",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (session) JsonSuccess(reply, { {"capture", SessionJson(*session, StateOf(*session))} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/entries", Perm(PERMISSION),
			"A capture's messages in order. Query: ?after=<seq> for only later ones, &limit=<1-5000> (default 2000); more is true when there are more",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto after = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "after")).value_or(0);
				const auto limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(PAGE_DEFAULT), 1, PAGE_MAX);
				nlohmann::json entries = nlohmann::json::array();
				for (const auto& record : Database::Get()->GetMessageCaptureEntries(session->id, after, limit)) entries.push_back(InspectorFormat::RecordJson(record));
				const bool more = entries.size() == limit;

				// Opening someone else's finished capture is audited (at most hourly per staff member and capture); starting one already was
				const auto* capture = Live(session->id);
				if (after == 0 && (!capture || capture->state == eState::ENDED) && session->startedById != context.accountId) {
					auto& last = g_ViewAudits[{ context.accountId, session->id }];
					if (last == Clock::time_point{} || Clock::now() - last > VIEW_AUDIT_INTERVAL) {
						last = Clock::now();
						Audit(context, "view_message_capture", "Opened the saved game messages of " + Describe(*session), AuditTarget::Character(session->characterId));
					}
				}
				JsonSuccess(reply, { {"capture", SessionJson(*session, StateOf(*session))}, {"entries", entries}, {"more", more} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/download", Perm(PERMISSION), "A capture and all its messages as a JSON file (audited)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				nlohmann::json entries = nlohmann::json::array();
				for (uint32_t after = 0;;) {
					const auto page = Database::Get()->GetMessageCaptureEntries(session->id, after, PAGE_MAX);
					for (const auto& record : page) entries.push_back(InspectorFormat::RecordJson(record));
					if (page.size() < PAGE_MAX) break;
					after = page.back().seq;
				}
				Audit(context, "export_message_capture", "Downloaded the game messages of " + Describe(*session), AuditTarget::Character(session->characterId));
				nlohmann::json file{ {"capture", SessionJson(*session, StateOf(*session))}, {"entries", std::move(entries)} };
				reply.status = eHTTPStatusCode::OK;
				reply.message = file.dump(1, '\t');
				reply.contentType = eContentType::APPLICATION_JSON;
				reply.headers.push_back("Content-Disposition: attachment; filename=\"capture_" + std::to_string(session->id) + ".json\"");
			});

		Route(eHTTPMethod::POST, "/api/inspector/sessions/:id/delete", Perm(PERMISSION), "Delete a finished capture and its messages (audited). Body: {reason}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto session = FindSession(context, reply);
				if (!session) return;
				if (const auto* capture = Live(session->id); capture && capture->state != eState::ENDED) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "Stop the capture before deleting it");
				}
				Database::Get()->DeleteMessageCaptureSession(session->id);
				g_Captures.erase(static_cast<uint32_t>(session->id));
				const auto reason = body->value("reason", std::string{});
				Audit(context, "delete_message_capture", "Deleted the saved game messages of " + Describe(*session) + (reason.empty() ? "" : ": " + reason),
					AuditTarget::Character(session->characterId));
				JsonSuccess(reply);
			});
	}
}
