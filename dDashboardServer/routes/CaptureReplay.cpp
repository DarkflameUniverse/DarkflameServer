#include "CaptureReplay.h"
#include "MasterPackets.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>

#include "BinaryPathFinder.h"
#include "CaptureBundle.h"
#include "CaptureProperty.h"
#include "CaptureTools.h"
#include "ClientAssets.h"
#include "Database.h"
#include "DashboardRoutes.h"
#include "Game.h"
#include "GameText.h"
#include "LiveWorld.h"
#include "Logger.h"
#include "master/MessageCapture.h"
#include "PacketDecoder.h"
#include "ReplicaDecoder.h"
#include "Workers.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "Web.h"
#include "dConfig.h"
#include "dServer.h"
#include "eHTTPMethod.h"

#ifndef DLU_GIT_COMMIT
#define DLU_GIT_COMMIT "unknown"
#endif

using namespace RouteUtils;
namespace fs = std::filesystem;

namespace {
	using Clock = std::chrono::steady_clock;
	using Session = IMessageCaptures::MessageCaptureSession;
	using json = nlohmann::json;

	constexpr const char* TOPIC = "packet_capture";
	constexpr const char* PERMISSION = "dev_message_inspector";
	constexpr uint32_t DEFAULT_SECONDS = 300;
	// Captures of everything at once (each gets every packet of every server)
	constexpr size_t MAX_EVERYTHING = 1;
	// Every server is told again this often (servers that started since, characters made since)
	constexpr auto REARM_INTERVAL = std::chrono::seconds(10);
	constexpr auto PUSH_INTERVAL = std::chrono::milliseconds(500);
	constexpr auto SAVE_INTERVAL = std::chrono::seconds(5);
	// Batches still on their way when a capture ends are kept this long
	constexpr auto LATE_GRACE = std::chrono::seconds(15);
	constexpr auto KEEP_ENDED = std::chrono::hours(1);
	// Most recent packets pushed to open pages
	constexpr size_t RECENT = 50;
	constexpr uint32_t PAGE_DEFAULT = 500;
	constexpr uint32_t PAGE_MAX = 2000;

	struct Capture {
		Session session;
		MessageCaptureControl control;
		Clock::time_point until;
		bool running{ true };
		Clock::time_point endedAt{};
		std::ofstream file;
		std::string pending;          // records waiting to be written
		Clock::time_point lastWrite{};
		Clock::time_point nextArm{};
		Clock::time_point lastSave{};
		std::deque<CaptureBundle::Record> recent;
		std::map<std::string, uint64_t> perSource;
		bool changed{};
		bool dirty{};
	};

	std::map<uint32_t, Capture> g_Captures;
	Clock::time_point g_NextPush{};

	// LOT -> components, read from the CDClient at startup (PreloadDecoding) and only read after: the replica pass runs
	// on worker threads, which never query the CDClient
	ReplicaDecoder::ComponentTable g_Components;

	// The last bundle read for the viewer (a running capture's file grows: read again when its size changed)
	struct Cached {
		uint64_t id{};
		uintmax_t size{};
		CaptureBundle::Bundle bundle;
		int64_t startUs{};

		// Replica packets' fields, read once per capture in timeline order (a serialization needs the construction
		// before it), on a worker thread
		std::once_flag decodeOnce;
		std::atomic<bool> decoded{};
		std::vector<std::optional<nlohmann::json>> replica;

		void Decode() {
			std::call_once(decodeOnce, [this] {
				ReplicaDecoder::Session session(g_Components);
				replica.resize(bundle.records.size());
				for (size_t i = 0; i < bundle.records.size(); i++) {
					const auto& record = bundle.records[i];
					if (record.header.flags & PacketRecordFlags::GAP || CaptureTools::FromClient(record.header)) continue;
					replica[i] = session.Decode(record.bytes, CaptureTools::ReplicaConnection(record.header));
				}
				decoded = true;
			});
		}

		const nlohmann::json* ReplicaFields(size_t i) const { return i < replica.size() && replica[i] ? &*replica[i] : nullptr; }

		// The properties the capture saw (CaptureProperty), read once after the replica pass, on a worker thread
		std::once_flag propertyOnce;
		std::atomic<bool> propertyBuilt{};
		nlohmann::json property;

		const nlohmann::json& Property() {
			Decode();
			std::call_once(propertyOnce, [this] {
				property = CaptureProperty::Build(bundle.records, startUs, [this](size_t i) { return ReplicaFields(i); });
				propertyBuilt = true;
			});
			return property;
		}
	};
	std::shared_ptr<Cached> g_Cache;

	bool Connected() { return Game::server && Game::server->GetIsConnectedToMaster(); }

	int64_t Setting(const std::string& key, int64_t fallback) {
		if (!Game::config) return fallback;
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	const char* TargetName(eCaptureTarget target) {
		switch (target) {
		case eCaptureTarget::CHARACTER: return "character";
		case eCaptureTarget::ACCOUNT: return "account";
		default: return "everything";
		}
	}

	std::string Describe(const Session& s) {
		if (s.target == "everything") return "everything";
		if (s.target == "character") return s.characterName + " (character " + std::to_string(s.characterId) + ")";
		return s.accountName + " (account " + std::to_string(s.accountId) + ")";
	}

	json SessionJson(const Capture& c) {
		const auto& s = c.session;
		return {
			{"id", s.id}, {"kind", 1}, {"target", s.target}, {"describe", Describe(s)},
			{"characterId", std::to_string(s.characterId)}, {"characterName", s.characterName},
			{"accountId", s.accountId}, {"accountName", s.accountName},
			{"startedBy", s.startedBy}, {"startedAt", s.startedAt}, {"endsAt", s.endsAt}, {"endedAt", s.endedAt},
			{"state", c.running ? "capturing" : "ended"}, {"endReason", s.endReason},
			{"received", s.messageCount}, {"bytes", s.byteCount}, {"dropped", s.dropped}, {"perSource", c.perSource},
		};
	}

	void SendControl(const MessageCaptureControl& control) {
		if (Connected()) MasterPackets::SendToMaster(control);
	}

	// Tell every server (again) what to record, for the time that is left
	void Arm(Capture& c) {
		const auto left = std::chrono::duration_cast<std::chrono::seconds>(c.until - Clock::now()).count();
		if (left <= 0) return;
		c.control.action = eMessageCaptureControl::ARM;
		c.control.seconds = static_cast<uint32_t>(left);
		// Characters made since the last time belong to the account too
		if (c.control.target == eCaptureTarget::ACCOUNT) {
			try {
				c.control.characterIds = Database::Get()->GetAccountCharacterIds(c.control.accountId);
			} catch (const std::exception&) {}
		}
		SendControl(c.control);
		c.nextArm = Clock::now() + REARM_INTERVAL;
	}

	void Write(Capture& c) {
		if (c.pending.empty()) return;
		if (c.file.is_open()) {
			c.file.write(c.pending.data(), static_cast<std::streamsize>(c.pending.size()));
			c.file.flush();
		}
		c.pending.clear();
		c.lastWrite = Clock::now();
	}

	void Save(Capture& c) {
		if (!c.dirty) return;
		try {
			Database::Get()->UpdateMessageCaptureSession(c.session);
		} catch (const std::exception& e) {
			LOG("Couldn't save packet capture %llu: %s", c.session.id, e.what());
		}
		c.dirty = false;
		c.lastSave = Clock::now();
	}

	void End(Capture& c, const std::string& reason) {
		if (!c.running) return;
		c.running = false;
		c.endedAt = Clock::now();
		c.session.endedAt = std::time(nullptr);
		c.session.endReason = reason;
		c.changed = c.dirty = true;
		c.control.action = eMessageCaptureControl::DISARM;
		SendControl(c.control);
		Write(c);
		Save(c);
		LOG("Packet capture %llu of %s ended: %s", c.session.id, Describe(c.session).c_str(), reason.c_str());
	}

	void Close(Capture& c) {
		Write(c);
		if (c.file.is_open()) c.file.close();
		Save(c);
	}

	void Push(Capture& c) {
		if (!c.changed) return;
		c.changed = false;
		json recent = json::array();
		const auto start = c.session.startedAt * 1000000;
		size_t i = c.session.messageCount - c.recent.size();
		for (const auto& record : c.recent) recent.push_back(CaptureTools::RecordJson(record, i++, start, false));
		c.recent.clear();
		json message{ {"capture", SessionJson(c)}, {"recent", recent} };
		Game::web.SendWSMessage(TOPIC, message);
	}

	void Note(Capture& c, const PacketRecordHeader& header, std::string_view bytes) {
		PacketRecord::Append(c.pending, header, bytes.data());
		c.session.messageCount++;
		c.session.byteCount += sizeof(header) + header.length;
		c.recent.push_back({ header, std::string(bytes) });
		if (c.recent.size() > RECENT) c.recent.pop_front();
		c.changed = c.dirty = true;
	}

	const char* SourceKey(uint8_t source) {
		switch (static_cast<eCaptureSource>(source)) {
		case eCaptureSource::AUTH: return "auth";
		case eCaptureSource::CHAT: return "chat";
		case eCaptureSource::WORLD: return "world";
		case eCaptureSource::MASTER: return "master";
		default: return "other";
		}
	}

	Capture* Live(uint64_t id) {
		const auto it = id <= UINT32_MAX ? g_Captures.find(static_cast<uint32_t>(id)) : g_Captures.end();
		return it == g_Captures.end() ? nullptr : &it->second;
	}

	// A packet capture's session from the path; writes the error when there is none
	std::optional<Session> FindSession(const HTTPContext& context, HTTPReply& reply) {
		const auto id = PathId<uint64_t>(context.path, 3);
		std::optional<Session> session;
		if (id) {
			if (auto* live = Live(*id)) {
				Write(*live);
				session = live->session;
			} else {
				session = Database::Get()->GetMessageCaptureSession(*id);
			}
		}
		if (!session || session->kind != 1) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such packet capture");
			return std::nullopt;
		}
		return session;
	}

	// The capture's records on one timeline (read again when the file grew)
	std::shared_ptr<Cached> Load(const Session& session, HTTPReply& reply) {
		const auto path = CaptureReplay::FileOf(session.id);
		std::error_code ec;
		const auto size = fs::file_size(path, ec);
		if (ec) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "The capture's file is gone (deleted, or kept on another machine)");
			return nullptr;
		}
		if (g_Cache && g_Cache->id == session.id && g_Cache->size == size) return g_Cache;
		auto loaded = std::make_shared<Cached>();
		auto& cached = *loaded;
		std::string error;
		if (!CaptureBundle::Load(path, cached.bundle, error)) {
			JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, error);
			return nullptr;
		}
		CaptureTools::SortTimeline(cached.bundle.records);
		cached.id = session.id;
		cached.size = size;
		cached.startUs = cached.bundle.records.empty() ? session.startedAt * 1000000 : std::min(session.startedAt * 1000000, cached.bundle.records.front().header.timeUs);
		g_Cache = std::move(loaded);
		return g_Cache;
	}

	std::string CharacterName(LWOOBJID id) {
		const auto info = id ? Database::Get()->GetCharacterInfo(id) : std::nullopt;
		return info ? info->name : "Character " + std::to_string(id);
	}

	// Each captured character's moves between world servers (CaptureTools::Worlds), with the zone's name from the locale
	json WorldsJson(const Cached& cached) {
		json out = json::array();
		std::map<LWOOBJID, std::string> names;
		for (const auto& visit : CaptureTools::Worlds(cached.bundle.records, cached.startUs)) {
			if (!names.contains(visit.characterId)) names[visit.characterId] = CharacterName(visit.characterId);
			out.push_back({ {"character", std::to_string(visit.characterId)}, {"name", names[visit.characterId]}, {"t", visit.t},
				{"zone", visit.zoneId}, {"zoneName", visit.zoneId ? GameText::ZoneName(visit.zoneId) : std::string{}},
				{"instance", visit.instanceId}, {"clone", visit.cloneId} });
		}
		return out;
	}

	json StoredSessionJson(const Session& s) {
		if (const auto* live = Live(s.id)) return SessionJson(*live);
		Capture view;
		view.session = s;
		view.running = false;
		return SessionJson(view);
	}

	// Checks and fills a new capture from the request; the error is written when it can't
	bool Prepare(const json& body, Capture& c, HTTPReply& reply) {
		const auto target = body.value("target", std::string{});
		auto& s = c.session;
		auto& control = c.control;
		if (target == "everything") {
			control.target = eCaptureTarget::EVERYTHING;
		} else if (target == "account") {
			control.target = eCaptureTarget::ACCOUNT;
			const auto text = body.value("account", std::string{});
			const auto number = GeneralUtils::TryParse<uint32_t>(text);
			json account = number ? Database::Get()->GetAccountById(*number) : json();
			if (!number) {
				if (const auto info = Database::Get()->GetAccountInfo(text)) account = Database::Get()->GetAccountById(info->id);
			}
			if (!account.is_object() || !account.contains("id")) {
				JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such account");
				return false;
			}
			control.accountId = account["id"].is_string() ? GeneralUtils::TryParse<uint32_t>(account["id"].get<std::string>()).value_or(0) : account["id"].get<uint32_t>();
			control.accountName = account.value("name", std::string{});
			control.characterIds = Database::Get()->GetAccountCharacterIds(control.accountId);
		} else if (target == "character") {
			control.target = eCaptureTarget::CHARACTER;
			const auto id = ResolveCharacter(body.value("character", std::string{}));
			const auto info = id ? Database::Get()->GetCharacterInfo(*id) : std::nullopt;
			if (!info) {
				JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such character");
				return false;
			}
			control.characterIds = { info->id };
			control.accountId = info->accountId;
			s.characterId = info->id;
			s.characterName = info->name;
		} else {
			JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "target must be account, character or everything");
			return false;
		}
		if (control.accountId) {
			s.accountId = control.accountId;
			const auto account = Database::Get()->GetAccountById(control.accountId);
			s.accountName = account.value("name", std::string{});
			// Account names are how logins and session keys are matched on the servers
			control.accountName = s.accountName;
		}
		s.target = TargetName(control.target);
		return true;
	}
}

namespace CaptureReplay {
	fs::path Folder() {
		auto folder = fs::path(Game::config ? Game::config->GetValue("capture_dir") : "");
		if (folder.empty()) folder = "captures";
		if (folder.is_relative()) folder = BinaryPathFinder::GetBinaryDir() / folder;
		return folder;
	}

	bool IsRunning(uint64_t sessionId) {
		const auto* c = Live(sessionId);
		return c && c->running;
	}

	fs::path FileOf(uint64_t sessionId) { return Folder() / (std::to_string(sessionId) + ".bundle"); }

	void Initialize() {
		// Packet captures don't carry on across a dashboard restart: the servers stop them at their time limit
		try {
			for (auto& session : Database::Get()->GetMessageCaptureSessions({ .unfinishedOnly = true, .ascending = true, .limit = 1000 })) {
				if (session.kind != 1) continue;
				session.endedAt = std::time(nullptr);
				session.endReason = "The dashboard stopped while it ran";
				Database::Get()->UpdateMessageCaptureSession(session);
			}
		} catch (const std::exception& e) {
			LOG("Couldn't end unfinished packet captures: %s", e.what());
		}
	}

	void HandleData(const MessageCaptureData& data) {
		std::set<uint32_t> unknown;
		const auto accept = [&](uint32_t id) -> Capture* {
			auto* c = id ? Live(id) : nullptr;
			if (c && (c->running || Clock::now() - c->endedAt < LATE_GRACE)) return c;
			if (id) unknown.insert(id);
			return nullptr;
		};
		PacketRecord::ForEach(data.packets, [&](const PacketRecordHeader& header, std::string_view bytes) {
			for (uint8_t bit = 0; bit < MessageCapture::MAX_SLOTS; bit++) {
				if (!(header.mask & (1 << bit))) continue;
				auto* c = accept(data.slots[bit]);
				if (!c) continue;
				auto own = header;
				own.mask = 1; // one capture per file
				Note(*c, own, bytes);
				c->perSource[SourceKey(header.source)]++;
			}
		});
		// Lost on a server (its buffer was full): a gap in every capture it was recording for
		if (data.packetsDropped) {
			for (const auto id : data.slots) {
				auto* c = accept(id);
				if (!c) continue;
				PacketRecordHeader gap;
				gap.timeUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				gap.source = data.source;
				gap.flags = PacketRecordFlags::GAP;
				gap.bits = data.packetsDropped;
				gap.zoneId = static_cast<uint16_t>(data.zoneId);
				gap.instanceId = static_cast<uint16_t>(data.instanceId);
				PacketRecord::Append(c->pending, gap, nullptr);
				c->session.dropped += data.packetsDropped;
				c->changed = c->dirty = true;
			}
		}
		// A server still recording for a capture that ended here (it missed the disarm): tell it again
		for (const auto id : unknown) {
			for (uint8_t slot = 0; slot < MessageCapture::MAX_SLOTS; slot++) {
				if (data.slots[slot] != id) continue;
				MessageCaptureControl control;
				control.action = eMessageCaptureControl::DISARM;
				control.captureId = id;
				control.slot = slot;
				SendControl(control);
			}
		}
	}

	void Update() {
		if (g_Captures.empty()) return;
		const auto now = Clock::now();
		const auto flushBytes = static_cast<size_t>(Setting("capture_flush_bytes", 256 * 1024));
		const auto flushInterval = std::chrono::milliseconds(Setting("capture_flush_interval_ms", 1000));
		const bool push = now >= g_NextPush;
		if (push) g_NextPush = now + PUSH_INTERVAL;
		for (auto it = g_Captures.begin(); it != g_Captures.end();) {
			auto& c = it->second;
			if (c.running) {
				if (now >= c.until) End(c, "Time limit reached");
				else if (now >= c.nextArm) Arm(c);
			}
			// One write per capture when enough is waiting or it waited long enough; never per packet
			if (c.pending.size() >= flushBytes || (!c.pending.empty() && now - c.lastWrite >= flushInterval)) Write(c);
			if (c.dirty && now - c.lastSave >= SAVE_INTERVAL) Save(c);
			if (push) Push(c);
			if (!c.running && c.file.is_open() && now - c.endedAt >= LATE_GRACE) Close(c);
			if (!c.running && now - c.endedAt >= KEEP_ENDED) it = g_Captures.erase(it);
			else ++it;
		}
	}

	void Shutdown() {
		for (auto& [id, c] : g_Captures) {
			if (c.running) End(c, "The dashboard stopped");
			Close(c);
		}
	}

	void PreloadDecoding() {
		const auto lots = ReplicaDecoder::LoadComponentTable(g_Components);
		LOG("Read the components of %zu LOTs for decoding replica packets", lots);
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription(TOPIC, std::function<uint8_t()>([] { return Permissions::Level(PERMISSION); }));

		Route(eHTTPMethod::GET, "/inspector/packets", Perm(PERMISSION), "Packet captures: arm, play back, export",
			[](HTTPReply& reply, const HTTPContext& context) {
				RenderPage(reply, context, "packet-captures.jinja2", "inspector");
			});

		Route(eHTTPMethod::GET, "/api/inspector/targets", Perm(PERMISSION),
			"Accounts and characters to capture, by part of an account or character name, or an account or character ID. Query: ?q=",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto q = QueryValue(context.queryString, "q");
				std::set<std::string> online;
				for (const auto& player : LiveWorld::OnlinePlayers()) online.insert(player.value("id", std::string{}));
				json accounts = json::array();
				const auto add = [&](uint32_t accountId, const std::string& name) {
					json characters = json::array();
					bool anyOnline = false;
					for (const auto& character : Database::Get()->GetAccountCharacters(accountId)) {
						const auto id = character.value("id", std::string{});
						const bool on = online.contains(id);
						anyOnline |= on;
						characters.push_back({ {"id", id}, {"name", character.value("name", std::string{})}, {"online", on} });
					}
					accounts.push_back({ {"id", accountId}, {"name", name}, {"online", anyOnline}, {"characters", characters} });
				};
				// A pasted character object ID finds its account
				const auto characterId = GeneralUtils::TryParse<LWOOBJID>(q);
				if (characterId && *characterId > UINT32_MAX) {
					if (const auto info = Database::Get()->GetCharacterInfo(*characterId)) {
						add(info->accountId, Database::Get()->GetAccountById(info->accountId).value("name", std::string{}));
					}
				} else {
					const auto table = Database::Get()->GetAccountsTable(0, 10, q, 1, true);
					for (const auto& row : table.value("data", json::array())) {
						const auto id = row["id"].is_number() ? row["id"].get<uint32_t>() : GeneralUtils::TryParse<uint32_t>(row["id"].get<std::string>()).value_or(0);
						add(id, row.value("name", std::string{}));
					}
				}
				JsonSuccess(reply, { {"accounts", accounts} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/packet-captures", Perm(PERMISSION), "Running and recently finished packet captures",
			[](HTTPReply& reply, const HTTPContext&) {
				json captures = json::array();
				for (auto it = g_Captures.rbegin(); it != g_Captures.rend(); ++it) captures.push_back(SessionJson(it->second));
				JsonSuccess(reply, { {"captures", captures}, {"maxSeconds", MessageCapture::MAX_SECONDS}, {"defaultSeconds", DEFAULT_SECONDS},
					{"slots", MessageCapture::MAX_SLOTS} });
			});

		Route(eHTTPMethod::POST, "/api/inspector/packet-captures", Perm(PERMISSION),
			"Arm a packet capture on every server (audited). Body: {target: account|character|everything, account (name or ID), character (name or ID), seconds}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto seconds = body->value("seconds", DEFAULT_SECONDS);
				if (seconds < 1 || seconds > MessageCapture::MAX_SECONDS) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A capture can run from 1 second to " + std::to_string(MessageCapture::MAX_SECONDS / 60) + " minutes");
				}
				Capture c;
				if (!Prepare(*body, c, reply)) return;

				// A free slot (each running capture has one bit in the servers' records)
				std::set<uint8_t> taken;
				size_t everything = 0;
				for (const auto& [id, other] : g_Captures) {
					if (!other.running) continue;
					taken.insert(other.control.slot);
					if (other.control.target == eCaptureTarget::EVERYTHING) everything++;
					const bool same = other.control.target == c.control.target && other.control.accountId == c.control.accountId &&
						other.session.characterId == c.session.characterId;
					if (same) return JsonError(reply, eHTTPStatusCode::CONFLICT, "That is already being captured (capture " + std::to_string(id) + ")");
				}
				if (c.control.target == eCaptureTarget::EVERYTHING && everything >= MAX_EVERYTHING) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "Everything is already being captured");
				}
				uint8_t slot = 0;
				while (slot < MessageCapture::MAX_SLOTS && taken.contains(slot)) slot++;
				if (slot == MessageCapture::MAX_SLOTS) return JsonError(reply, eHTTPStatusCode::CONFLICT, "At most " + std::to_string(MessageCapture::MAX_SLOTS) + " packet captures can run at once");
				if (!Connected()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");

				auto& s = c.session;
				s.kind = 1;
				s.startedById = context.accountId;
				s.startedBy = context.authenticatedUser;
				s.startedAt = std::time(nullptr);
				s.endsAt = s.startedAt + seconds;
				s.id = Database::Get()->InsertMessageCaptureSession(s);
				if (s.id == 0 || s.id > UINT32_MAX) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't save the capture");

				std::error_code ec;
				fs::create_directories(CaptureReplay::Folder(), ec);
				c.file.open(CaptureReplay::FileOf(s.id), std::ios::binary | std::ios::trunc);
				if (!c.file) {
					Database::Get()->DeleteMessageCaptureSession(s.id);
					return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't create the capture file in " + CaptureReplay::Folder().string());
				}
				const json meta{ {"format", CaptureBundle::FORMAT_VERSION}, {"origin", "dlu-capture"}, {"captureId", s.id}, {"target", s.target},
					{"server", { {"version", PROJECT_VERSION}, {"commit", DLU_GIT_COMMIT} }}, {"startedAt", s.startedAt}, {"portable", false} };
				const auto header = CaptureBundle::Header(meta);
				c.file.write(header.data(), static_cast<std::streamsize>(header.size()));
				c.file.flush();

				c.control.captureId = static_cast<uint32_t>(s.id);
				c.control.slot = slot;
				c.until = Clock::now() + std::chrono::seconds(seconds);
				c.lastWrite = c.lastSave = Clock::now();
				Arm(c);
				const auto summary = SessionJson(c);
				const auto target = s.characterId ? AuditTarget::Character(s.characterId) : AuditTarget::Account(s.accountId);
				Audit(context, "arm_packet_capture", "Armed a packet capture of " + Describe(s) + " for " + std::to_string(seconds) + " s (capture " +
					std::to_string(s.id) + ")", target);
				g_Captures.emplace(c.control.captureId, std::move(c));
				JsonSuccess(reply, { {"capture", summary} });
			});

		Route(eHTTPMethod::POST, "/api/inspector/packet-captures/:id/stop", Perm(PERMISSION), "Stop a running packet capture",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint32_t>(context.path, 3);
				auto* c = id ? Live(*id) : nullptr;
				if (!c || !c->running) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such running packet capture");
				End(*c, "Stopped by " + context.authenticatedUser);
				Push(*c);
				Audit(context, "stop_packet_capture", "Stopped the packet capture of " + Describe(c->session) + " (capture " + std::to_string(c->session.id) + ")");
				JsonSuccess(reply, { {"capture", SessionJson(*c)} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/packets", Perm(PERMISSION),
			"A packet capture's packets on one timeline, decoded. Query: ?offset=&limit=<1-2000>&q=<part of a name>&source=auth|chat|world|master&fields=0",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				const auto offset = GeneralUtils::TryParse<size_t>(QueryValue(context.queryString, "offset")).value_or(0);
				const auto limit = std::clamp<size_t>(GeneralUtils::TryParse<size_t>(QueryValue(context.queryString, "limit")).value_or(PAGE_DEFAULT), 1, PAGE_MAX);
				auto q = QueryValue(context.queryString, "q");
				for (auto& ch : q) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
				const auto source = QueryValue(context.queryString, "source");
				const bool fields = QueryValue(context.queryString, "fields") != "0";
				// Read on the main thread (the session JSON reads live capture state); the packets are decoded on a
				// worker the first time, since replica packets need a pass over the whole capture
				auto capture = StoredSessionJson(*session);
				Workers::Reply(reply, context, cached->decoded, [cached, capture = std::move(capture), offset, limit, q, source, fields](HTTPReply& out) {
					if (fields) cached->Decode();
					json records = json::array();
					size_t matched = 0;
					const auto& all = cached->bundle.records;
					for (size_t i = 0; i < all.size(); i++) {
						if (q.empty() && source.empty()) {
							if (i < offset) continue;
							if (records.size() >= limit) break;
							records.push_back(CaptureTools::RecordJson(all[i], i, cached->startUs, fields, cached->ReplicaFields(i)));
							continue;
						}
						auto record = CaptureTools::RecordJson(all[i], i, cached->startUs, false);
						if (!source.empty() && record.value("source", std::string{}) != source) continue;
						if (!q.empty() && record.value("name", std::string{}).find(q) == std::string::npos) continue;
						if (matched++ < offset || records.size() >= limit) continue;
						records.push_back(fields ? CaptureTools::RecordJson(all[i], i, cached->startUs, true, cached->ReplicaFields(i)) : record);
					}
					JsonSuccess(out, { {"capture", capture}, {"total", all.size()}, {"matched", q.empty() && source.empty() ? all.size() : matched},
						{"start", cached->startUs / 1000}, {"duration", all.empty() ? 0.0 : static_cast<double>(all.back().header.timeUs - cached->startUs) / 1000.0},
						{"records", records} });
				});
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/packets/:index", Perm(PERMISSION), "One captured packet with its bytes",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				const auto index = PathId<size_t>(context.path, 5);
				if (!index || *index >= cached->bundle.records.size()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such packet");
				Workers::Reply(reply, context, cached->decoded, [cached, index = *index](HTTPReply& out) {
					cached->Decode();
					const auto& record = cached->bundle.records[index];
					auto json = CaptureTools::RecordJson(record, index, cached->startUs, true, cached->ReplicaFields(index));
					json["hex"] = MessageCapture::ToHex(record.bytes);
					JsonSuccess(out, { {"record", json} });
				});
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/positions", Perm(PERMISSION),
			"Where the captured characters moved, in the World 3D replay's shape, and which world each was on when (worlds). Query: ?zone=<zone>|all (default: the first zone with movement)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				const auto tracks = CaptureTools::Tracks(cached->bundle.records, cached->startUs);
				std::set<uint32_t> zones;
				for (const auto& t : tracks) zones.insert(t.zoneId);
				const bool all = QueryValue(context.queryString, "zone") == "all";
				auto zone = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "zone")).value_or(zones.empty() ? 0 : *zones.begin());
				json players = json::array();
				for (const auto& t : tracks) {
					if (!all && t.zoneId != zone) continue;
					players.push_back({ {"id", std::to_string(t.characterId) + ":" + std::to_string(t.instanceId)}, {"character", std::to_string(t.characterId)},
						{"name", CharacterName(t.characterId)}, {"zone", t.zoneId}, {"instances", json::array({ t.instanceId })}, {"samples", t.samples} });
				}
				const auto from = cached->startUs / 1000000;
				const auto to = cached->bundle.records.empty() ? from + 1 : cached->bundle.records.back().header.timeUs / 1000000 + 1;
				JsonSuccess(reply, { {"zone", all ? json(nullptr) : json(zone)}, {"zones", zones}, {"from", from}, {"to", to}, {"players", players},
					{"worlds", WorldsJson(*cached)}, {"idleSeconds", 3}, {"bucket", 1}, {"interval", 1}, {"truncated", false} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/property", Perm(PERMISSION),
			"The properties the capture saw, per world (zone, instance, clone): {worlds: [{zone, zoneName, instance, clone, saved, info, counts, "
			"models: [{object, lot, spawner, ugcId, behaviors, spans: [{from, to, i, position, rotation}]}], events}]}. From the captured replica "
			"packets and property game messages; `saved` is the property saved for that zone and clone now (its id for links and UGC meshes)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				// On the main thread: zone names and the saved property of each world the capture was on
				std::map<std::pair<uint32_t, uint32_t>, json> saved;
				std::map<uint32_t, std::string> zoneNames;
				for (const auto& record : cached->bundle.records) {
					const auto& h = record.header;
					if (h.source != static_cast<uint8_t>(eCaptureSource::WORLD) || !h.zoneId || saved.contains({ h.zoneId, h.cloneId })) continue;
					saved[{ h.zoneId, h.cloneId }] = nullptr;
					if (!zoneNames.contains(h.zoneId)) zoneNames[h.zoneId] = GameText::ZoneName(h.zoneId);
					const auto info = h.cloneId ? Database::Get()->GetPropertyInfo(h.zoneId, h.cloneId) : std::nullopt;
					if (!info) continue;
					saved[{ h.zoneId, h.cloneId }] = { {"id", std::to_string(info->id)}, {"name", info->name}, {"ownerId", std::to_string(info->ownerId)},
						{"ownerName", CharacterName(info->ownerId)} };
				}
				Workers::Reply(reply, context, cached->propertyBuilt, [cached, saved = std::move(saved), zoneNames = std::move(zoneNames)](HTTPReply& out) {
					json worlds = cached->Property()["worlds"];
					for (auto& world : worlds) {
						const auto zone = world["zone"].get<uint32_t>(), clone = world["clone"].get<uint32_t>();
						const auto name = zoneNames.find(zone);
						world["zoneName"] = name == zoneNames.end() ? std::string{} : name->second;
						const auto found = saved.find({ zone, clone });
						world["saved"] = found == saved.end() ? json(nullptr) : found->second;
						// Names from the locale, in the viewer's language (read once at startup, so workers may read them)
						for (auto& model : world["models"]) model["name"] = ClientAssets::ItemName(model["lot"].get<LOT>());
						for (auto& event : world["events"]) {
							if (event.contains("lot")) event["name"] = ClientAssets::ItemName(event["lot"].get<LOT>());
						}
					}
					JsonSuccess(out, { {"worlds", worlds} });
				});
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/worlds", Perm(PERMISSION),
			"Which world server each captured character was on, in time order: one entry per move to another zone or instance (zone 0 is character select)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				JsonSuccess(reply, { {"worlds", WorldsJson(*cached)} });
			});

		Route(eHTTPMethod::GET, "/api/inspector/sessions/:id/bundle", Perm(PERMISSION),
			"Export a packet capture as a portable bundle for the capture tool (audited). Query: ?anonymise=1 also blanks character names and chat",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto session = FindSession(context, reply);
				if (!session) return;
				const auto cached = Load(*session, reply);
				if (!cached) return;
				const bool anonymise = QueryValue(context.queryString, "anonymise") == "1";
				auto bundle = cached->bundle;
				bundle.meta["origin"] = "dlu-capture";
				bundle.meta["exportedAt"] = std::time(nullptr);
				bundle.meta["server"] = { {"version", PROJECT_VERSION}, {"commit", DLU_GIT_COMMIT} };
				// Zone and client data checksums seen in the capture, so a replay on other data is reported, not diffed
				json zones = json::object();
				for (const auto& record : bundle.records) {
					const auto decoded = PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header));
					if (!decoded.fields) continue;
					if (decoded.name == "LOAD_STATIC_ZONE") zones[std::to_string((*decoded.fields)["mapID"].get<int>())] = (*decoded.fields)["mapChecksum"];
					if (decoded.name == "VALIDATION") bundle.meta["fdbChecksum"] = (*decoded.fields)["fdbChecksum"];
				}
				bundle.meta["zones"] = zones;
				const auto characters = CaptureTools::MakePortable(bundle);
				if (anonymise) CaptureTools::Anonymise(bundle);
				// The setup section: what a replay needs to make these characters on another server (their saved data,
				// without the account it belonged to)
				json setup = json::array();
				static const std::regex account(R"( acct="[0-9]+")");
				for (const auto& [symbol, id] : characters) {
					const auto info = Database::Get()->GetCharacterInfo(id);
					if (!info) continue;
					auto xml = Database::Get()->GetCharacterXml(id);
					xml = std::regex_replace(xml, account, "");
					setup.push_back({ {"symbol", symbol}, {"placeholder", bundle.meta["ids"][symbol]["placeholder"]}, {"name", anonymise ? symbol : info->name}, {"xml", xml} });
				}
				bundle.meta["setup"] = { {"characters", setup} };
				std::string out = CaptureBundle::Header(bundle.meta);
				for (const auto& record : bundle.records) CaptureBundle::AppendRecord(out, record);
				Audit(context, "export_packet_capture", "Exported the packet capture of " + Describe(*session) + (anonymise ? " (anonymised)" : "") + " as a bundle",
					session->characterId ? AuditTarget::Character(session->characterId) : AuditTarget::Account(session->accountId));
				reply.status = eHTTPStatusCode::OK;
				reply.message = std::move(out);
				reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
				reply.headers.push_back("Content-Disposition: attachment; filename=\"capture_" + std::to_string(session->id) + (anonymise ? "_anonymised" : "") + ".bundle\"");
			});
	}
}
