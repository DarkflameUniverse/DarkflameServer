#include "ZonePublishRoutes.h"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <openssl/rand.h>

#include "ClientAssets.h"
#include "Database.h"
#include "Game.h"
#include "GameText.h"
#include "GeneralUtils.h"
#include "IHTTPMiddleware.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "MultipartFile.h"
#include "Permissions.h"
#include "ReportRoutes.h"
#include "RouteUtils.h"
#include "ServerState.h"
#include "Web.h"
#include "ZoneEditLock.h"
#include "ZoneOverrideStore.h"
#include "ZonePaths.h"
#include "ZonePublish.h"
#include "ZonePublishAuth.h"
#include "dConfig.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "master/WorldFiles.h"

using namespace RouteUtils;
using ZoneEditLock::eReload;
using ZoneEditLock::eResult;

namespace {
	constexpr const char* BASE = "/api/v1/zones/:zoneid";
	constexpr const char* AUDIT_ACTION = "zone_publish";
	constexpr const char* ADMIN_PERMISSION = "zone_editors_manage";
	constexpr size_t ZONE_SEGMENT = 3; // /api/v1/zones/<zone>
	constexpr size_t MAX_NOTE = 500;
	constexpr size_t MAX_RECORDS = 500;
	constexpr uint64_t MIB = 1024 * 1024;
	// A multipart body is its files plus a little framing per part
	constexpr uint64_t FRAMING_BYTES = 1 * MIB;
	constexpr auto RELOAD_TIMEOUT = std::chrono::minutes(10);
	// Refused requests per address before the address is refused outright for a while
	constexpr uint32_t MAX_FAILURES = 10;
	constexpr int64_t FAILURE_WINDOW = 600;
	constexpr int64_t READ_AUDIT_SECONDS = 60;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	// ---- Settings (main thread; the worker gets copies) ----

	uint32_t ConfigNumber(const char* key, uint32_t fallback) {
		if (!Game::config) return fallback;
		return GeneralUtils::TryParse<uint32_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	std::filesystem::path Root() {
		return ZoneOverrideStore::RootFromSetting(Game::config ? Game::config->GetValue("zone_override_dir") : std::string());
	}

	ZoneEditLock::Levels Levels() {
		ZoneEditLock::Levels levels;
		levels.publish = static_cast<uint8_t>(std::clamp<uint32_t>(ConfigNumber("zone_publish_gm_level", ZoneEditLock::DEFAULT_PUBLISH_LEVEL), 1, 9));
		levels.admin = static_cast<uint8_t>(std::clamp<uint32_t>(ConfigNumber("zone_lock_admin_gm_level", ZoneEditLock::DEFAULT_ADMIN_LEVEL), 1, 9));
		return levels;
	}

	ZonePublish::Limits Limits() {
		ZonePublish::Limits limits;
		limits.maxFileBytes = static_cast<uint64_t>(std::max<uint32_t>(ConfigNumber("zone_publish_max_file_mb", 64), 1)) * MIB;
		limits.maxTotalBytes = static_cast<uint64_t>(std::max<uint32_t>(ConfigNumber("zone_publish_max_total_mb", 256), 1)) * MIB;
		limits.maxFiles = std::max<uint32_t>(ConfigNumber("zone_publish_max_files", 128), 1);
		limits.maxObjects = std::max<uint32_t>(ConfigNumber("zone_publish_max_objects", 100000), 1);
		return limits;
	}

	uint32_t KeepBackups() { return ConfigNumber("zone_publish_max_backups", 20); }

	// ---- Replies: {"error": code, "message": text} ----

	void Error(HTTPReply& reply, eHTTPStatusCode status, const std::string& code, const std::string& message, nlohmann::json extra = nlohmann::json::object()) {
		extra["error"] = code;
		extra["message"] = message;
		JsonReply(reply, status, extra);
	}

	// A name as it was sent, with bytes that are not text replaced (a reply has to be valid UTF-8)
	std::string SafeText(const std::string& text) {
		return nlohmann::json::parse(nlohmann::json(text).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)).get<std::string>();
	}

	std::string NewId() {
		unsigned char bytes[8];
		if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("RAND_bytes failed");
		static constexpr char digits[] = "0123456789abcdef";
		std::string id;
		for (const auto byte : bytes) {
			id += digits[byte >> 4];
			id += digits[byte & 15];
		}
		return id;
	}

	std::string AccountName(uint32_t accountId) {
		const auto account = Database::Get()->GetAccountById(accountId);
		return account.contains("error") ? "account " + std::to_string(accountId) : account.value("name", std::string());
	}

	// ---- Audit: every use of a key, and every refusal ----

	std::map<std::string, int64_t> g_ReadAudited; // key id, zone and action -> when last written
	uint64_t g_Audited = 0; // entries written (or, for reads, counted as written), to tell a request that left none

	// `read`: something that only looks (asked for again and again while a reload runs), written at most once a minute
	void AuditUse(const HTTPContext& context, uint32_t zoneId, const std::string& action, const std::string& result, bool read = false) {
		const auto keyId = context.apiKey ? context.apiKey->keyId : 0;
		const auto text = "key " + std::to_string(keyId) + ", zone " + std::to_string(zoneId) + ", " + action + ": " + result;
		g_Audited++;
		if (read) {
			const auto now = Now();
			if (g_ReadAudited.size() > 5000) g_ReadAudited.clear();
			auto& last = g_ReadAudited[std::to_string(keyId) + "/" + std::to_string(zoneId) + "/" + action];
			if (now - last < READ_AUDIT_SECONDS) {
				LOG_DEBUG("[zone api] %s: %s", context.authenticatedUser.c_str(), text.c_str());
				return;
			}
			last = now;
		}
		Audit(context, AUDIT_ACTION, text);
	}

	// ---- The gate every route goes through first ----

	ZonePublishAuth::FailureLimiter g_Failures(MAX_FAILURES, FAILURE_WINDOW);

	std::string LastForwardedFor(const HTTPContext& context) {
		const auto& forwarded = context.GetHeader("X-Forwarded-For");
		const auto comma = forwarded.rfind(',');
		auto last = comma == std::string::npos ? forwarded : forwarded.substr(comma + 1);
		last.erase(0, last.find_first_not_of(' '));
		last.erase(last.find_last_not_of(' ') + 1);
		return last;
	}

	std::string ActionOf(const HTTPContext& context) {
		std::string action = context.method;
		for (size_t i = ZONE_SEGMENT + 1; ; ++i) {
			const auto segment = PathSegment(context.path, i);
			if (segment.empty()) break;
			action += " " + std::string(segment);
		}
		return action;
	}

	// A request that was refused without anything saying so in the audit log gets its entry here: no refusal goes unwritten
	void AuditIfRefused(const HTTPContext& context, const HTTPReply& reply, uint64_t auditedBefore) {
		if (g_Audited != auditedBefore || reply.deferred || static_cast<uint32_t>(reply.status) < 400) return;
		const auto body = nlohmann::json::parse(reply.message, nullptr, false);
		const auto code = body.is_object() ? body.value("error", std::string("error")) : std::string("error");
		AuditUse(context, PathId<uint32_t>(context.path, ZONE_SEGMENT).value_or(0), ActionOf(context),
			"refused (" + code + ", " + std::to_string(static_cast<uint32_t>(reply.status)) + ")");
	}

	class ZoneApiGate : public IHTTPMiddleware {
	public:
		bool Process(HTTPContext& context, HTTPReply& reply) override {
			const auto address = ClientAddress(context);
			const auto now = Now();
			ZonePublishAuth::Request request;
			request.blocked = g_Failures.Blocked(address, now);
			request.transportAllowed = ZonePublishAuth::TransportAllowed(ConfigFlag("behind_proxy", false), context.clientIP,
				LastForwardedFor(context), context.GetHeader("X-Forwarded-Proto"));
			request.authenticated = context.isAuthenticated;
			request.key = context.apiKey.get();
			request.gmLevel = context.gmLevel; // read from the database for this request (ApiKeyService)
			request.requiredLevel = Levels().publish;
			request.changes = context.method != "GET";
			const auto denial = ZonePublishAuth::Check(request);
			if (denial == ZonePublishAuth::eDenial::NONE) return true;

			Error(reply, ZonePublishAuth::Status(denial), ZonePublishAuth::Code(denial), ZonePublishAuth::Message(denial));
			// Already refused outright: said once, when the address was blocked
			if (denial == ZonePublishAuth::eDenial::RATE_LIMITED) return false;
			const bool nowBlocked = g_Failures.RecordFailure(address, now);
			const auto zoneId = PathId<uint32_t>(context.path, ZONE_SEGMENT).value_or(0);
			auto who = context;
			if (!who.isAuthenticated) who.authenticatedUser = "[no key] " + address.substr(0, 45);
			std::string result = std::string("refused (") + ZonePublishAuth::Code(denial) + ") from " + address;
			if (nowBlocked) result += "; the address is refused for the next " + std::to_string(FAILURE_WINDOW / 60) + " minutes";
			AuditUse(who, zoneId, ActionOf(context), result);
			return false;
		}
		std::string GetName() const override { return "ZoneApiGate"; }
	};

	// ---- Zones ----

	struct ZoneRef {
		uint32_t zoneId{};
		std::string luzPath; // relative to res/maps/, lowercase, forward slashes
		std::string folder;
	};

	std::optional<ZoneRef> ZoneOf(uint32_t zoneId) {
		auto luz = ZoneLuzPath(zoneId);
		if (!luz) return std::nullopt;
		std::replace(luz->begin(), luz->end(), '\\', '/');
		ZoneRef zone{ zoneId, ZonePublish::Canonical(*luz), {} };
		zone.folder = ZonePublish::FolderOf(zone.luzPath);
		// A zone whose files are not in a folder of their own under maps/ can't be given a set
		if (ZonePublish::NameProblem(zone.luzPath) || zone.folder.empty()) return std::nullopt;
		return zone;
	}

	// The zone a request names and the override folder, or the error reply
	std::optional<ZoneRef> ZoneFor(const HTTPContext& context, HTTPReply& reply, std::filesystem::path* root = nullptr) {
		const auto zoneId = PathId<uint32_t>(context.path, ZONE_SEGMENT);
		const auto zone = zoneId ? ZoneOf(*zoneId) : std::nullopt;
		if (!zone) {
			Error(reply, eHTTPStatusCode::NOT_FOUND, "unknown_zone", "There is no such zone");
			return std::nullopt;
		}
		if (root) {
			*root = Root();
			if (root->empty()) {
				Error(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "disabled", "Edited zone files are turned off on this server (zone_override_dir is empty)");
				return std::nullopt;
			}
		}
		return zone;
	}

	uint32_t PlayersIn(uint32_t zoneId, uint32_t* instances = nullptr) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		uint32_t players = 0;
		for (const auto& world : ServerState::g_WorldInstances) {
			if (world.mapID != zoneId) continue;
			players += world.players;
			if (instances) (*instances)++;
		}
		return players;
	}

	// ---- Locks ----

	ZoneEditLock::Actor ActorOf(const HTTPContext& context, uint32_t zoneId) {
		return { context.accountId, context.gmLevel, Database::Get()->IsZoneEditor(zoneId, context.accountId) };
	}

	std::optional<uint32_t> HolderOf(const std::optional<IZoneEditing::ZoneLock>& lock) {
		return lock ? std::optional<uint32_t>(lock->accountId) : std::nullopt;
	}

	// Whether the caller holds the zone's lock; writes the 403 otherwise
	bool RequireLock(const HTTPContext& context, uint32_t zoneId, HTTPReply& reply, const std::string& action) {
		const auto lock = Database::Get()->GetZoneLock(zoneId);
		const auto result = ZoneEditLock::Change(ActorOf(context, zoneId), HolderOf(lock), Levels());
		if (result == eResult::OK) return true;
		if (result == eResult::LEVEL_TOO_LOW) {
			Error(reply, eHTTPStatusCode::FORBIDDEN, "gm_level", "Your account's GM level is too low for zone files");
		} else {
			nlohmann::json extra{ {"holder", lock ? nlohmann::json(AccountName(lock->accountId)) : nlohmann::json(nullptr)} };
			if (lock) extra["since"] = lock->lockedAt;
			extra["holder_is_you"] = false;
			Error(reply, eHTTPStatusCode::FORBIDDEN, "lock_required", lock ? "Another account holds this zone's lock" : "Take this zone's lock first", extra);
		}
		AuditUse(context, zoneId, action, "refused (" + reply.message.substr(0, 120) + ")");
		return false;
	}

	nlohmann::json LockJson(uint32_t zoneId, uint32_t callerAccountId) {
		const auto lock = Database::Get()->GetZoneLock(zoneId);
		nlohmann::json editors = nlohmann::json::array();
		for (const auto& editor : Database::Get()->GetZoneEditors(zoneId)) editors.push_back(AccountName(editor.accountId));
		return { {"holder", lock ? nlohmann::json(AccountName(lock->accountId)) : nlohmann::json(nullptr)},
			{"since", lock ? nlohmann::json(lock->lockedAt) : nlohmann::json(nullptr)},
			{"holder_is_you", lock && lock->accountId == callerAccountId}, {"editors", editors} };
	}

	// ---- The worker: file work only, its result handed back to the main thread ----

	using Completion = std::function<void()>;
	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<std::function<Completion()>> g_Jobs;
	std::deque<Completion> g_Done;
	bool g_Stopping = false;

	void PostMain(Completion done) {
		std::lock_guard lock(g_Mutex);
		g_Done.push_back(std::move(done));
	}

	void WorkerLoop() {
		while (true) {
			std::function<Completion()> job;
			{
				std::unique_lock lock(g_Mutex);
				g_Wake.wait(lock, [] { return g_Stopping || !g_Jobs.empty(); });
				if (g_Stopping) return;
				job = std::move(g_Jobs.front());
				g_Jobs.pop_front();
			}
			auto done = job();
			if (done) PostMain(std::move(done));
		}
	}

	void Submit(std::function<Completion()> job) {
		{
			std::lock_guard lock(g_Mutex);
			if (!g_Worker.joinable()) {
				g_Stopping = false;
				g_Worker = std::thread(WorkerLoop);
			}
			g_Jobs.push_back(std::move(job));
		}
		g_Wake.notify_one();
	}

	// ---- What was asked for, by id ----

	struct Record {
		std::string id;
		uint32_t zoneId{};
		std::string action; // publish, restore, clear
		std::string state = "validating";
		uint32_t accountId{};
		std::string user;
		bool reloadWanted = true;
		bool movePlayers{};
		uint32_t instances{};
		uint32_t playersMoved{};
		std::vector<std::string> errors;
		std::string error, message; // why it failed
		int64_t finishedAt{};
		ZonePublishRoutes::ReloadCounters before;
		std::chrono::steady_clock::time_point reloadAt;
		int64_t createdAt{};
	};

	std::map<std::string, Record> g_Records;
	std::map<uint32_t, std::string> g_Active;   // zone -> the change being checked or written
	std::set<uint32_t> g_Waiting;               // zones whose files changed and whose reload hasn't been asked for
	std::map<uint32_t, ZonePublishRoutes::ReloadCounters> g_Counters; // master's last status, per zone
	std::map<uint32_t, std::string> g_ZoneMessages;

	bool Finished(const Record& record) { return record.state == "done" || record.state == "failed"; }

	Record& NewRecord(const std::string& id, uint32_t zoneId, const std::string& action, const HTTPContext& context) {
		if (g_Records.size() >= MAX_RECORDS) {
			// The oldest finished ones go
			std::vector<std::pair<int64_t, std::string>> finished;
			for (const auto& [key, record] : g_Records) if (Finished(record)) finished.emplace_back(record.createdAt, key);
			std::ranges::sort(finished);
			for (size_t i = 0; i < finished.size() && g_Records.size() >= MAX_RECORDS; ++i) g_Records.erase(finished[i].second);
		}
		auto& record = g_Records[id];
		record.id = id;
		record.zoneId = zoneId;
		record.action = action;
		record.accountId = context.accountId;
		record.user = context.authenticatedUser;
		record.createdAt = Now();
		return record;
	}

	void Finish(Record& record, bool ok) {
		record.state = ok ? "done" : "failed";
		record.finishedAt = Now();
	}

	void Fail(Record& record, const std::string& error, const std::string& message) {
		record.error = error;
		record.message = message;
		Finish(record, false);
	}

	nlohmann::json RecordJson(const Record& record) {
		nlohmann::json json{ {"publishId", record.id}, {"zoneId", record.zoneId}, {"action", record.action}, {"state", record.state},
			{"players", PlayersIn(record.zoneId)},
			{"reload", { {"instances", record.instances}, {"playersMoved", record.playersMoved}, {"errors", record.errors} }},
			{"finishedAt", record.finishedAt ? nlohmann::json(record.finishedAt) : nlohmann::json(nullptr)} };
		if (record.state == "failed") {
			json["error"] = record.error.empty() ? "failed" : record.error;
			json["message"] = record.message;
		}
		return json;
	}

	bool MasterConnected() { return Game::server && Game::server->GetIsConnectedToMaster(); }

	void SendHold(uint32_t zoneId, bool hold) {
		if (!MasterConnected()) return;
		WorldReloadHold message;
		message.zoneId = zoneId;
		message.hold = hold;
		MasterPackets::SendToMaster(message);
	}

	// Ask master to replace the zone's instances now (the existing world reload)
	void StartReload(Record& record) {
		g_Waiting.erase(record.zoneId);
		uint32_t instances = 0;
		PlayersIn(record.zoneId, &instances);
		if (instances == 0) {
			// Nothing runs: the next instance reads the new files as it starts
			SendHold(record.zoneId, false);
			return Finish(record, true);
		}
		const auto counters = g_Counters.find(record.zoneId);
		record.before = counters == g_Counters.end() ? ZonePublishRoutes::ReloadCounters{} : counters->second;
		WorldReloadRequest request;
		request.zoneId = record.zoneId;
		request.requestedBy = ("zone files, " + record.user).substr(0, WorldFiles::MAX_BY);
		MasterPackets::SendToMaster(request);
		record.state = "reloading";
		record.reloadAt = std::chrono::steady_clock::now();
	}

	// After a zone's files changed: reload now, hold while players are there, or leave it
	eReload DecideReload(Record& record) {
		const auto players = PlayersIn(record.zoneId);
		const auto plan = ZoneEditLock::PlanReload(record.reloadWanted, players, record.movePlayers);
		if (plan == eReload::STARTED) {
			StartReload(record);
		} else {
			// Master goes on leaving the zone's instances alone until a reload is asked for
			g_Waiting.insert(record.zoneId);
			if (plan == eReload::HELD) record.state = "held";
			else Finish(record, true);
		}
		return plan;
	}

	// Nothing was changed after all: master watches the zone again, unless an earlier set still waits
	void ReleaseHold(uint32_t zoneId) {
		if (!g_Waiting.contains(zoneId)) SendHold(zoneId, false);
	}

	nlohmann::json FilesJson(const std::vector<ZoneOverrideStore::Entry>& files) {
		nlohmann::json out = nlohmann::json::array();
		for (const auto& file : files) out.push_back({ {"path", file.path}, {"bytes", file.bytes}, {"sha256", file.sha256}, {"version", file.version} });
		return out;
	}

	// Reads "reload" and "ifPlayersPresent" as a form field or JSON member gave them; false for a value that is neither
	bool ReadFlag(const std::string& text, bool fallback, bool& out) {
		if (text.empty()) { out = fallback; return true; }
		if (text == "true" || text == "1") { out = true; return true; }
		if (text == "false" || text == "0") { out = false; return true; }
		return false;
	}

	bool ReadPlayersRule(const std::string& text, bool& move) {
		if (text.empty() || text == "hold") { move = false; return true; }
		if (text == "move") { move = true; return true; }
		return false;
	}

	std::string JsonText(const nlohmann::json& body, const char* key) {
		const auto it = body.find(key);
		if (it == body.end() || it->is_null()) return "";
		if (it->is_string()) return it->get<std::string>();
		if (it->is_boolean()) return it->get<bool>() ? "true" : "false";
		return "?";
	}

	// What the worker did with a change
	struct Outcome {
		eHTTPStatusCode status = eHTTPStatusCode::OK;
		std::string error, message;       // set for anything but a set that was checked
		bool validated{};
		ZonePublish::Result validation;
		bool dryRun{};
		bool applied{};
		ZoneOverrideStore::Applied result;
		bool reloadWanted = true;
		bool movePlayers{};
		std::string note;
	};

	// Main thread: answer the request the worker finished, and start (or hold) the reload
	void Complete(const std::string& id, const DeferredReply& deferred, const HTTPContext& context, const Outcome& outcome, const std::filesystem::path& root) {
		ZoneOverrideStore::RemoveStaging(root, id);
		const auto it = g_Records.find(id);
		if (it == g_Records.end()) return;
		auto& record = it->second;
		if (const auto active = g_Active.find(record.zoneId); active != g_Active.end() && active->second == id) g_Active.erase(active);
		const auto zoneId = record.zoneId;
		const auto action = record.action;
		HTTPReply reply;

		if (!outcome.error.empty()) {
			Error(reply, outcome.status, outcome.error, outcome.message);
			AuditUse(context, zoneId, action, "failed (" + outcome.error + ": " + outcome.message + ")");
			ReleaseHold(zoneId);
			// A request still waiting gets the error as its answer; one that was given its id finds it in the status
			if (!deferred) return Fail(record, outcome.error, outcome.message);
			g_Records.erase(it);
			return deferred.Send(std::move(reply));
		}
		if (outcome.validated && !outcome.validation.ok) {
			nlohmann::json files = nlohmann::json::array();
			std::string codes;
			for (const auto& file : outcome.validation.files) {
				if (file.ok) continue;
				nlohmann::json errors = nlohmann::json::array();
				for (const auto& error : file.errors) {
					nlohmann::json entry{ {"code", error.code}, {"message", error.message} };
					if (error.offset) entry["offset"] = *error.offset;
					errors.push_back(std::move(entry));
					if (codes.find(error.code) == std::string::npos) codes += (codes.empty() ? "" : ", ") + error.code;
				}
				files.push_back({ {"path", SafeText(file.path)}, {"ok", false}, {"errors", errors} });
			}
			JsonReply(reply, eHTTPStatusCode::UNPROCESSABLE_ENTITY, { {"error", "validation_failed"}, {"files", files} });
			AuditUse(context, zoneId, action, "refused (validation_failed: " + codes + "); nothing was written");
			ReleaseHold(zoneId);
			g_Records.erase(it);
			return deferred.Send(std::move(reply));
		}
		if (outcome.dryRun) {
			nlohmann::json files = nlohmann::json::array();
			for (const auto& file : outcome.validation.files) files.push_back({ {"path", file.path}, {"bytes", file.bytes}, {"sha256", file.sha256}, {"version", file.version} });
			JsonReply(reply, eHTTPStatusCode::OK, { {"zoneId", zoneId}, {"dryRun", true}, {"files", files}, {"reload", { {"state", "skipped"} }} });
			AuditUse(context, zoneId, action, "dry run passed (" + std::to_string(files.size()) + " files); nothing was written");
			ReleaseHold(zoneId);
			g_Records.erase(it);
			return deferred.Send(std::move(reply));
		}

		record.reloadWanted = outcome.reloadWanted;
		record.movePlayers = outcome.movePlayers;
		const auto plan = DecideReload(record);
		JsonReply(reply, eHTTPStatusCode::OK, { {"publishId", id}, {"zoneId", zoneId},
			{"backupId", outcome.result.backupId ? nlohmann::json(*outcome.result.backupId) : nlohmann::json(nullptr)},
			{"files", FilesJson(outcome.result.files)}, {"reload", { {"state", ZoneEditLock::ReloadName(plan)} }} });
		AuditUse(context, zoneId, action, "applied as " + id + " (" + std::to_string(outcome.result.files.size()) + " files" +
			(outcome.result.backupId ? ", the set before is backup " + *outcome.result.backupId : "") + "), reload " + ZoneEditLock::ReloadName(plan) +
			(outcome.note.empty() ? "" : "; note: " + outcome.note.substr(0, 200)));
		if (deferred) deferred.Send(std::move(reply));
	}

	// The checks every change starts with, on the main thread; the change's id, or nothing with the reply written
	std::optional<ZoneRef> BeginChange(const HTTPContext& context, HTTPReply& reply, const std::string& action, std::filesystem::path& root) {
		const auto zone = ZoneFor(context, reply, &root);
		if (!zone) return std::nullopt;
		if (!RequireLock(context, zone->zoneId, reply, action)) return std::nullopt;
		if (g_Active.contains(zone->zoneId)) {
			Error(reply, eHTTPStatusCode::CONFLICT, "publish_in_progress", "This zone's files are being changed right now; try again when that is done");
			AuditUse(context, zone->zoneId, action, "refused (publish_in_progress)");
			return std::nullopt;
		}
		if (!MasterConnected()) {
			Error(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "master_unavailable", "The dashboard is not connected to the master server");
			return std::nullopt;
		}
		return zone;
	}

	// ---- POST publish: the precheck at the headers, then the body ----

	// Runs before any of the body is taken: the zone, the lock, and one change at a time
	class PublishPrecheck : public IHTTPMiddleware {
	public:
		bool Process(HTTPContext& context, HTTPReply& reply) override {
			std::filesystem::path root;
			const auto audited = g_Audited;
			if (!BeginChange(context, reply, "publish", root)) {
				AuditIfRefused(context, reply, audited);
				return false;
			}
			if (!MultipartFile::Boundary(context.GetHeader("Content-Type"))) {
				Error(reply, eHTTPStatusCode::BAD_REQUEST, "malformed", "Send the files as multipart/form-data");
				AuditIfRefused(context, reply, audited);
				return false;
			}
			context.userData["publish_id"] = NewId();
			return true;
		}
		std::string GetName() const override { return "ZonePublishPrecheck"; }
	};

	std::string SpoolFile(const HTTPContext& context) {
		const auto id = context.userData.find("publish_id");
		if (id == context.userData.end()) return "";
		try {
			return (ZoneOverrideStore::MakeUploadDir(Root(), id->second) / "body").string();
		} catch (const std::exception& ex) {
			LOG("Zone files: no room for an upload: %s", ex.what());
			return "";
		}
	}

	void HandlePublish(HTTPReply& reply, const HTTPContext& context) {
		const auto id = context.userData.at("publish_id");
		std::filesystem::path root;
		// The upload took a while: the zone's lock and its one change at a time are looked at again
		const auto zone = BeginChange(context, reply, "publish", root);
		const auto boundary = MultipartFile::Boundary(context.GetHeader("Content-Type"));
		if (!zone || !boundary) {
			ZoneOverrideStore::RemoveStaging(Root(), id);
			return;
		}
		NewRecord(id, zone->zoneId, "publish", context);
		g_Active[zone->zoneId] = id;
		// Master leaves the zone's instances alone from before the files change until the reload is decided
		SendHold(zone->zoneId, true);

		ZonePublish::Zone target;
		target.zoneId = zone->zoneId;
		target.luzPath = zone->luzPath;
		// The scenes the zone has in the client's files: a set can't add any
		if (const auto luz = ClientAssets::ReadResFile("maps/" + zone->luzPath)) {
			for (const auto& scene : ZonePaths::ReadSceneFiles(*luz)) target.clientScenes.insert(zone->folder + ZonePublish::Canonical(scene));
		}
		const auto clientMaps = ClientAssets::ResFolder() / "maps";
		target.hasFile = [clientMaps](const std::string& path) {
			std::error_code code;
			return std::filesystem::is_regular_file(clientMaps / path, code);
		};
		const auto limits = Limits();
		const auto keep = KeepBackups();
		const auto who = context.authenticatedUser;
		const auto now = Now();
		const auto body = std::filesystem::path(context.bodyFile);
		const auto deferred = Web::Defer(reply, context);

		Submit([=]() -> Completion {
			Outcome outcome;
			const auto fail = [&](eHTTPStatusCode status, const char* error, const std::string& message) {
				outcome.status = status;
				outcome.error = error;
				outcome.message = message;
			};
			try {
				std::vector<MultipartFile::Part> parts;
				MultipartFile::Limits split;
				split.maxParts = limits.maxFiles + 16;
				split.maxFileBytes = limits.maxFileBytes;
				const auto error = MultipartFile::Split(body, *boundary, body.parent_path(), split, parts);
				std::error_code code;
				std::filesystem::remove(body, code);
				std::string reload, dryRun, players;
				std::vector<ZonePublish::Upload> uploads;
				bool strayFile = false;
				for (const auto& part : parts) {
					if (part.isFile && part.name != "file") strayFile = true;
					else if (part.isFile) uploads.push_back({ part.filename, part.file, part.bytes });
					else if (part.name == "reload") reload = part.value;
					else if (part.name == "dryRun") dryRun = part.value;
					else if (part.name == "ifPlayersPresent") players = part.value;
					else if (part.name == "note") outcome.note = part.value.substr(0, MAX_NOTE);
				}
				if (error == MultipartFile::eError::TOO_MANY_PARTS) fail(eHTTPStatusCode::PAYLOAD_TOO_LARGE, "too_large", "The set has more files than this server takes");
				else if (error == MultipartFile::eError::IO) fail(eHTTPStatusCode::INTERNAL_SERVER_ERROR, "storage", "The upload could not be read");
				else if (error != MultipartFile::eError::NONE) fail(eHTTPStatusCode::BAD_REQUEST, "malformed", "The body is not well formed multipart/form-data");
				else if (strayFile) fail(eHTTPStatusCode::BAD_REQUEST, "malformed", "Every file part's form field name is \"file\"");
				else if (!ReadFlag(reload, true, outcome.reloadWanted) || !ReadFlag(dryRun, false, outcome.dryRun) || !ReadPlayersRule(players, outcome.movePlayers)) {
					fail(eHTTPStatusCode::BAD_REQUEST, "malformed", "reload and dryRun are true or false; ifPlayersPresent is hold or move");
				}
				if (!outcome.error.empty()) return [=] { Complete(id, deferred, context, outcome, root); };

				outcome.validation = ZonePublish::Validate(target, uploads, limits);
				outcome.validated = true;
				if (outcome.validation.ok && !outcome.dryRun) {
					PostMain([id] { if (const auto it = g_Records.find(id); it != g_Records.end()) it->second.state = "applying"; });
					std::vector<ZoneOverrideStore::NewFile> files;
					for (const auto& file : outcome.validation.files) files.push_back({ file.path, file.source, file.bytes, file.sha256, file.version });
					outcome.result = ZoneOverrideStore::Apply(root, { target.zoneId, ZonePublish::FolderOf(target.luzPath), id, who, outcome.note, now, keep }, files);
					outcome.applied = true;
				}
			} catch (const ZoneOverrideStore::StoreError& ex) {
				fail(eHTTPStatusCode::INTERNAL_SERVER_ERROR, "storage", ex.what());
			} catch (const std::exception&) {
				fail(eHTTPStatusCode::INTERNAL_SERVER_ERROR, "internal", "The set could not be checked");
			}
			return [=] { Complete(id, deferred, context, outcome, root); };
		});
	}

	// Restore a backup or clear the zone's set: the same swap, and the same reload
	void HandleStoreChange(HTTPReply& reply, const HTTPContext& context, const std::string& action) {
		std::filesystem::path root;
		const auto zone = BeginChange(context, reply, action, root);
		if (!zone) return;
		const auto body = ParseBody(context);
		Outcome request;
		if (!body || !ReadFlag(JsonText(*body, "reload"), true, request.reloadWanted) || !ReadPlayersRule(JsonText(*body, "ifPlayersPresent"), request.movePlayers)) {
			return Error(reply, eHTTPStatusCode::BAD_REQUEST, "malformed", "Send JSON; reload is true or false, ifPlayersPresent is hold or move");
		}
		std::string backupId;
		if (action == "restore") {
			backupId = JsonText(*body, "backupId");
			if (!ZoneOverrideStore::ValidId(backupId)) return Error(reply, eHTTPStatusCode::BAD_REQUEST, "malformed", "Send the backupId to restore");
			const auto backups = ZoneOverrideStore::Backups(root, zone->zoneId);
			if (std::ranges::none_of(backups, [&](const auto& backup) { return backup.id == backupId; })) {
				AuditUse(context, zone->zoneId, action, "refused (no backup " + backupId + ")");
				return Error(reply, eHTTPStatusCode::NOT_FOUND, "unknown_backup", "This zone has no such backup");
			}
		}
		const auto id = NewId();
		NewRecord(id, zone->zoneId, action, context).state = "applying";
		g_Active[zone->zoneId] = id;
		SendHold(zone->zoneId, true);
		const ZoneOverrideStore::Change change{ zone->zoneId, zone->folder, id, context.authenticatedUser,
			action == "restore" ? "restored " + backupId : "", Now(), KeepBackups() };
		// Answered with its id now; how it goes is the status' to say
		const DeferredReply deferred;
		JsonReply(reply, eHTTPStatusCode::OK, { {"publishId", id}, {"zoneId", zone->zoneId} });
		Submit([=]() -> Completion {
			Outcome outcome = request;
			try {
				outcome.result = action == "restore" ? ZoneOverrideStore::Restore(root, change, backupId) : ZoneOverrideStore::Clear(root, change);
				outcome.applied = true;
				if (action == "restore") outcome.note = "backup " + backupId;
			} catch (const std::exception& ex) {
				outcome.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				outcome.error = "storage";
				outcome.message = ex.what();
			}
			return [=] { Complete(id, deferred, context, outcome, root); };
		});
	}

	// The record a request names, when it is this zone's; writes the 404 otherwise
	Record* RecordFor(const HTTPContext& context, HTTPReply& reply, uint32_t zoneId) {
		const auto id = std::string(PathSegment(context.path, ZONE_SEGMENT + 2));
		const auto it = g_Records.find(id);
		if (it == g_Records.end() || it->second.zoneId != zoneId) {
			Error(reply, eHTTPStatusCode::NOT_FOUND, "unknown_publish", "This zone has no such publish (they are forgotten when the dashboard restarts)");
			return nullptr;
		}
		return &it->second;
	}

	void Register(eHTTPMethod method, const std::string& path, const std::string& description, Handler handler) {
		Game::web.RegisterHTTPRoute({ .path = path, .method = method, .middleware = { std::make_shared<ZoneApiGate>() },
			.handle = [handler = std::move(handler)](HTTPReply& reply, const HTTPContext& context) {
				const auto audited = g_Audited;
				try {
					handler(reply, context);
				} catch (const std::exception& ex) {
					LOG("Error handling %s %s: %s", context.method.c_str(), context.path.c_str(), ex.what());
					Error(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "internal", "Internal server error");
				}
				AuditIfRefused(context, reply, audited);
			} });
		Document(method, path, ZonePublishAuth::SCOPE, description);
	}

	// ---- The dashboard's own page: editors and locks (a signed-in admin, not the API) ----

	bool RequireAdmin(const HTTPContext& context, HTTPReply& reply) {
		if (context.gmLevel >= Levels().admin) return true;
		JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Managing zone editors and locks needs GM level " + std::to_string(Levels().admin) + " (zone_lock_admin_gm_level)");
		return false;
	}

	nlohmann::json AdminJson() {
		std::map<uint32_t, nlohmann::json> zones;
		const auto zoneRow = [&zones](uint32_t zoneId) -> nlohmann::json& {
			auto [it, added] = zones.try_emplace(zoneId);
			if (added) it->second = { {"zone", zoneId}, {"zoneName", GameText::ZoneName(zoneId)}, {"holder", nullptr}, {"holderId", 0}, {"since", nullptr},
				{"editors", nlohmann::json::array()}, {"state", ""} };
			return it->second;
		};
		for (const auto& editor : Database::Get()->GetZoneEditors(0)) {
			zoneRow(editor.zoneId)["editors"].push_back({ {"accountId", editor.accountId}, {"name", AccountName(editor.accountId)},
				{"assignedBy", editor.assignedBy}, {"assignedAt", editor.assignedAt} });
		}
		for (const auto& lock : Database::Get()->GetZoneLocks()) {
			auto& row = zoneRow(lock.zoneId);
			row["holder"] = AccountName(lock.accountId);
			row["holderId"] = lock.accountId;
			row["since"] = lock.lockedAt;
		}
		for (const auto zoneId : g_Waiting) zoneRow(zoneId)["state"] = "waiting for its reload";
		for (const auto& [zoneId, id] : g_Active) zoneRow(zoneId)["state"] = "being changed";
		nlohmann::json rows = nlohmann::json::array();
		for (auto& [_, row] : zones) rows.push_back(std::move(row));
		const auto levels = Levels();
		return { {"enabled", !Root().empty()}, {"zones", rows}, {"maxEditors", ZoneEditLock::MAX_EDITORS},
			{"publishLevel", levels.publish}, {"adminLevel", levels.admin} };
	}
}

namespace ZonePublishRoutes {
	void HandleStatus(const WorldFilesStatus& status) {
		g_Counters.clear();
		g_ZoneMessages.clear();
		for (const auto& zone : status.zones) {
			g_Counters[zone.zoneId] = { true, zone.requests, zone.acted, zone.playersMoved, zone.failed, zone.pending };
			g_ZoneMessages[zone.zoneId] = zone.message;
		}
		for (auto& [id, record] : g_Records) {
			if (record.state != "reloading") continue;
			const auto counters = g_Counters.find(record.zoneId);
			const auto progress = ProgressOf(record.before, counters == g_Counters.end() ? ReloadCounters{} : counters->second);
			record.instances = std::max(record.instances, progress.instances);
			record.playersMoved = std::max(record.playersMoved, progress.playersMoved);
			if (progress.failed > 0) {
				const auto message = std::to_string(progress.failed) + " instance(s) could not be replaced: " + g_ZoneMessages[record.zoneId];
				if (record.errors.empty()) record.errors.push_back(message);
				else record.errors.back() = message;
			}
			if (!progress.done) continue;
			// Nothing was replaced and something could not be: the new files are not loaded
			if (progress.failed > 0 && progress.instances == 0) Fail(record, "reload_failed", record.errors.back());
			else Finish(record, true);
		}
	}

	void Update() {
		std::deque<Completion> done;
		{
			std::lock_guard lock(g_Mutex);
			done.swap(g_Done);
		}
		for (auto& completion : done) {
			try {
				completion();
			} catch (const std::exception& ex) {
				LOG("Zone files: finishing a change failed: %s", ex.what());
			}
		}
		const auto now = std::chrono::steady_clock::now();
		for (auto& [id, record] : g_Records) {
			if (record.state != "reloading" || now - record.reloadAt < RELOAD_TIMEOUT) continue;
			record.errors.push_back("The master server did not say the reload finished");
			Fail(record, "reload_failed", record.errors.back());
		}
	}

	void Shutdown() {
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Wake.notify_all();
		if (g_Worker.joinable()) g_Worker.join();
	}

	void RegisterRoutes() {
		// What a stopped dashboard left half done was never swapped in
		ZoneOverrideStore::RemoveAllStaging(Root());
		const std::string base = BASE;

		Game::web.RegisterUploadRoute({ .path = base + "/publish", .method = eHTTPMethod::POST,
			.middleware = { std::make_shared<ZoneApiGate>(), std::make_shared<PublishPrecheck>() },
			.maxBytes = [] { return Limits().maxTotalBytes + FRAMING_BYTES; },
			.spoolFile = SpoolFile,
			.handle = [](HTTPReply& reply, const HTTPContext& context) {
				const auto audited = g_Audited;
				try {
					HandlePublish(reply, context);
				} catch (const std::exception& ex) {
					LOG("Error handling a zone publish: %s", ex.what());
					Error(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "internal", "Internal server error");
				}
				AuditIfRefused(context, reply, audited);
			} });
		Document(eHTTPMethod::POST, base + "/publish", ZonePublishAuth::SCOPE,
			"Send a zone's edited files (multipart). One file part per file, its filename the path relative to res/maps/ as the zone names it: the zone's "
			".luz, and any of its scenes' .lvl and .lutriggers files and its .raw. Every file is checked first and nothing is kept unless all pass (422 "
			"lists what is wrong, file by file). The set replaces the zone's whole earlier set, which becomes a backup. Needs the zone's lock. "
			"Body: {reload=true|false (default true: replace the zone's instances), dryRun=true|false (only check), note, ifPlayersPresent=hold|move "
			"(default hold: with players there the reload waits for .../reload)}. Returns {publishId, zoneId, backupId, files: [{path, bytes, sha256, version}], "
			"reload: {state: started|held|skipped}}");

		Register(eHTTPMethod::GET, base + "/publish/:publishid",
			"How a publish, restore or clear is going: {state: validating|applying|held|reloading|done|failed, players (in the zone now), "
			"reload: {instances, playersMoved, errors}, finishedAt}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = ZoneFor(context, reply);
				if (!zone) return;
				const auto* record = RecordFor(context, reply, zone->zoneId);
				if (!record) return;
				AuditUse(context, zone->zoneId, "status " + record->id, record->state, true);
				JsonReply(reply, eHTTPStatusCode::OK, RecordJson(*record));
			});

		Register(eHTTPMethod::POST, base + "/publish/:publishid/reload",
			"Go on with a reload that is held because players are in the zone. Needs the zone's lock. Body: {ifPlayersPresent=move|hold (hold only "
			"reloads when nobody is there any more)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = ZoneFor(context, reply);
				if (!zone) return;
				auto* record = RecordFor(context, reply, zone->zoneId);
				if (!record) return;
				if (!RequireLock(context, zone->zoneId, reply, "reload " + record->id)) return;
				const auto body = ParseBody(context);
				bool move = false;
				if (!body || !ReadPlayersRule(JsonText(*body, "ifPlayersPresent"), move)) return Error(reply, eHTTPStatusCode::BAD_REQUEST, "malformed", "ifPlayersPresent is hold or move");
				if (record->state != "held") return Error(reply, eHTTPStatusCode::CONFLICT, "not_held", "This publish is not waiting for its reload");
				if (!MasterConnected()) return Error(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "master_unavailable", "The dashboard is not connected to the master server");
				if (move || PlayersIn(zone->zoneId) == 0) StartReload(*record);
				AuditUse(context, zone->zoneId, "reload " + record->id, record->state == "held" ? "still held (players are there)" : "reload started" + std::string(move ? " (players are moved)" : ""));
				JsonReply(reply, eHTTPStatusCode::OK, RecordJson(*record));
			});

		Register(eHTTPMethod::GET, base + "/overrides",
			"The zone's edited files in use ({current: {files: [{path, bytes, sha256, publishedBy, publishedAt}]}, null for none) and the sets it had before "
			"({backups: [{backupId, createdAt, createdBy, note}]}, newest first)",
			[](HTTPReply& reply, const HTTPContext& context) {
				std::filesystem::path root;
				const auto zone = ZoneFor(context, reply, &root);
				if (!zone) return;
				nlohmann::json files = nlohmann::json::array(), backups = nlohmann::json::array();
				for (const auto& file : ZoneOverrideStore::Current(root, zone->zoneId, zone->folder)) {
					files.push_back({ {"path", file.path}, {"bytes", file.bytes}, {"sha256", file.sha256}, {"publishedBy", file.who}, {"publishedAt", file.when} });
				}
				for (const auto& backup : ZoneOverrideStore::Backups(root, zone->zoneId)) {
					backups.push_back({ {"backupId", backup.id}, {"createdAt", backup.time}, {"createdBy", backup.who}, {"note", backup.note} });
				}
				AuditUse(context, zone->zoneId, "overrides", "listed", true);
				JsonReply(reply, eHTTPStatusCode::OK, { {"current", files.empty() ? nlohmann::json(nullptr) : nlohmann::json{ {"files", files} }}, {"backups", backups} });
			});

		Register(eHTTPMethod::POST, base + "/overrides/restore",
			"Put one of the zone's earlier sets back (the set in use becomes a backup), then reload as a publish does. Returns {publishId} to ask the status with. Needs the zone's lock. "
			"Body: {backupId, reload=true|false, ifPlayersPresent=hold|move}",
			[](HTTPReply& reply, const HTTPContext& context) { HandleStoreChange(reply, context, "restore"); });

		Register(eHTTPMethod::DELETE, base + "/overrides",
			"Take the zone's edited files away, so the client's are read again (the set becomes a backup), then reload as a publish does. Returns "
			"{publishId} to ask the status with. Needs the zone's lock. Body (optional): {reload=true|false, ifPlayersPresent=hold|move}",
			[](HTTPReply& reply, const HTTPContext& context) { HandleStoreChange(reply, context, "clear"); });

		Register(eHTTPMethod::GET, base + "/lock", "Who holds the zone's lock and who its editors are: {holder (null: nobody), since, holder_is_you, editors: [names]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = ZoneFor(context, reply);
				if (!zone) return;
				AuditUse(context, zone->zoneId, "lock", "looked at", true);
				JsonReply(reply, eHTTPStatusCode::OK, LockJson(zone->zoneId, context.accountId));
			});

		Register(eHTTPMethod::POST, base + "/lock",
			"Take the zone's lock: one of its editors, or an admin (zone_lock_admin_gm_level). 409 {error: zone_locked, holder, since} when another account holds it. "
			"A lock is kept until it is released",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = ZoneFor(context, reply);
				if (!zone) return;
				const auto time = Now();
				auto lock = Database::Get()->GetZoneLock(zone->zoneId);
				auto result = ZoneEditLock::Take(ActorOf(context, zone->zoneId), HolderOf(lock), Levels());
				// Someone else got there between the look and the take
				if (result == eResult::OK && !Database::Get()->TakeZoneLock(zone->zoneId, context.accountId, time)) {
					lock = Database::Get()->GetZoneLock(zone->zoneId);
					result = lock && lock->accountId == context.accountId ? eResult::NO_CHANGE : eResult::LOCKED_BY_OTHER;
				}
				switch (result) {
				case eResult::OK:
				case eResult::NO_CHANGE: {
					AuditUse(context, zone->zoneId, "lock", result == eResult::OK ? "taken" : "already held");
					auto json = LockJson(zone->zoneId, context.accountId);
					json["result"] = static_cast<uint8_t>(result);
					return JsonReply(reply, eHTTPStatusCode::OK, json);
				}
				case eResult::LEVEL_TOO_LOW:
					Error(reply, eHTTPStatusCode::FORBIDDEN, "gm_level", "Your account's GM level is too low for zone files", { {"result", 1} });
					break;
				case eResult::NOT_ASSIGNED:
					Error(reply, eHTTPStatusCode::FORBIDDEN, "not_an_editor", "Your account is not one of this zone's editors", { {"result", 2} });
					break;
				case eResult::LOCKED_BY_OTHER:
					Error(reply, eHTTPStatusCode::CONFLICT, "zone_locked", "Another account holds this zone's lock",
						{ {"holder", lock ? AccountName(lock->accountId) : std::string()}, {"since", lock ? lock->lockedAt : 0}, {"holder_is_you", false}, {"result", 3} });
					break;
				}
				AuditUse(context, zone->zoneId, "lock", "refused (" + nlohmann::json::parse(reply.message).value("error", std::string()) + ")");
			});

		Register(eHTTPMethod::DELETE, base + "/lock", "Release the zone's lock: its holder, or an admin (zone_lock_admin_gm_level) taking it away from whoever holds it",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = ZoneFor(context, reply);
				if (!zone) return;
				const auto lock = Database::Get()->GetZoneLock(zone->zoneId);
				const auto result = ZoneEditLock::Release(ActorOf(context, zone->zoneId), HolderOf(lock), Levels());
				if (result == eResult::OK) {
					Database::Get()->ReleaseZoneLock(zone->zoneId);
					const bool forced = lock->accountId != context.accountId;
					AuditUse(context, zone->zoneId, "unlock", forced ? "taken away from " + AccountName(lock->accountId) : "released");
				} else if (result == eResult::LEVEL_TOO_LOW) {
					AuditUse(context, zone->zoneId, "unlock", "refused (gm_level)");
					return Error(reply, eHTTPStatusCode::FORBIDDEN, "gm_level", "Your account's GM level is too low for zone files", { {"result", 1} });
				} else if (result == eResult::LOCKED_BY_OTHER) {
					AuditUse(context, zone->zoneId, "unlock", "refused (held by " + AccountName(lock->accountId) + ")");
					return Error(reply, eHTTPStatusCode::FORBIDDEN, "lock_required", "Another account holds this zone's lock",
						{ {"holder", AccountName(lock->accountId)}, {"since", lock->lockedAt}, {"holder_is_you", false}, {"result", 3} });
				} else {
					AuditUse(context, zone->zoneId, "unlock", "nothing was locked");
				}
				auto json = LockJson(zone->zoneId, context.accountId);
				json["result"] = static_cast<uint8_t>(result);
				JsonReply(reply, eHTTPStatusCode::OK, json);
			});

		// The Worlds page's card
		Route(eHTTPMethod::GET, "/api/zone-editing", Perm(ADMIN_PERMISSION),
			"Every zone that has editors or a held lock: its editors, who holds its lock since when, and whether its files wait for a reload",
			[](HTTPReply& reply, const HTTPContext&) { JsonSuccess(reply, AdminJson()); });

		Route(eHTTPMethod::POST, "/api/zone-editing/:zone/editors", Perm(ADMIN_PERMISSION),
			"Make an account one of a zone's editors (up to 10 per zone). Body: {account (its name or ID)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!RequireAdmin(context, reply)) return;
				const auto zoneId = PathId<uint32_t>(context.path, 2);
				if (!zoneId || !ZoneOf(*zoneId)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "There is no such zone");
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				std::string text = (*body).contains("account") && (*body)["account"].is_number_unsigned() ? std::to_string((*body)["account"].get<uint32_t>()) : body->value("account", std::string());
				uint32_t accountId = 0;
				if (const auto asId = GeneralUtils::TryParse<uint32_t>(text); asId && !Database::Get()->GetAccountById(*asId).contains("error")) accountId = *asId;
				else if (const auto info = Database::Get()->GetAccountInfo(text)) accountId = info->id;
				if (accountId == 0) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account not found");
				if (Database::Get()->IsZoneEditor(*zoneId, accountId)) return JsonSuccess(reply, { {"message", "Already one of its editors"} });
				if (Database::Get()->GetZoneEditors(*zoneId).size() >= ZoneEditLock::MAX_EDITORS) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "A zone has at most " + std::to_string(ZoneEditLock::MAX_EDITORS) + " editors");
				}
				Database::Get()->AddZoneEditor({ *zoneId, accountId, context.authenticatedUser.substr(0, 64), Now() });
				Audit(context, "zone_editor_add", "Made " + AccountName(accountId) + " an editor of zone " + std::to_string(*zoneId), AuditTarget::Account(accountId));
				JsonSuccess(reply, { {"message", "Editor added"} });
			});

		Route(eHTTPMethod::POST, "/api/zone-editing/:zone/editors/remove", Perm(ADMIN_PERMISSION),
			"Take an account off a zone's editors (a lock it holds stays until it is released). Body: {accountId}",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!RequireAdmin(context, reply)) return;
				const auto zoneId = PathId<uint32_t>(context.path, 2);
				const auto body = ParseBody(context);
				if (!zoneId || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid request");
				const auto accountId = body->value("accountId", 0u);
				if (!Database::Get()->RemoveZoneEditor(*zoneId, accountId)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not one of its editors");
				Audit(context, "zone_editor_remove", "Took " + AccountName(accountId) + " off the editors of zone " + std::to_string(*zoneId), AuditTarget::Account(accountId));
				JsonSuccess(reply, { {"message", "Editor removed"} });
			});

		Route(eHTTPMethod::POST, "/api/zone-editing/:zone/release", Perm(ADMIN_PERMISSION),
			"Take a zone's lock away from whoever holds it",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!RequireAdmin(context, reply)) return;
				const auto zoneId = PathId<uint32_t>(context.path, 2);
				if (!zoneId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				const auto lock = Database::Get()->GetZoneLock(*zoneId);
				if (!lock || !Database::Get()->ReleaseZoneLock(*zoneId)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Nobody holds this zone's lock");
				Audit(context, "zone_lock_release", "Took the lock of zone " + std::to_string(*zoneId) + " away from " + AccountName(lock->accountId), AuditTarget::Account(lock->accountId));
				JsonSuccess(reply, { {"message", "Lock released"} });
			});
	}
}
