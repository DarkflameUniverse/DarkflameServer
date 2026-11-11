#include "ZoneEditor.h"

#include <algorithm>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

#include "AssetManager.h"
#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "User.h"
#include "UserManager.h"
#include "ZoneEditLock.h"
#include "ZoneFile.h"
#include "ZoneFilePack.h"
#include "ZoneFilePush.h"
#include "ZoneOverrideStore.h"
#include "ZonePublish.h"
#include "dConfig.h"
#include "dServer.h"
#include "master/WorldFiles.h"

using ZoneEditLock::eResult;
using ZoneFilePack::eFileType;

namespace {
	constexpr const char* AUDIT_ACTION = "zone_publish";
	constexpr uint64_t MIB = 1024 * 1024;
	// How long a client has to send its files after it was told it may
	constexpr int64_t PUSH_GRANT_SECONDS = 600;
	// What a requester is told holds the lock when nobody does (it has to lock the zone first)
	constexpr const char16_t* NOBODY = u"nobody (lock the map first)";

	struct SystemAddressLess {
		bool operator()(const SystemAddress& a, const SystemAddress& b) const {
			return a.binaryAddress != b.binaryAddress ? a.binaryAddress < b.binaryAddress : a.port < b.port;
		}
	};

	// A push the client was told it may send
	struct Grant {
		uint8_t fileType{};
		int32_t levelFileId{};
		int64_t until{};
	};

	std::set<SystemAddress, SystemAddressLess> g_Editing;
	std::map<SystemAddress, Grant, SystemAddressLess> g_Grants;
	bool g_Changing = false; // this zone's files are being checked or written here
	uint32_t g_NextChange = 0;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	uint32_t ConfigNumber(const char* key, uint32_t fallback) {
		return GeneralUtils::TryParse<uint32_t>(Game::config->GetValue(key)).value_or(fallback);
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

	std::filesystem::path Root() {
		return ZoneOverrideStore::RootFromSetting(Game::config->GetValue("zone_override_dir"));
	}

	// ---- Who is asking ----

	struct Requester {
		User* user{};
		ZoneEditLock::Actor actor;
		std::string name;
	};

	/**
	 * The account behind a connection, as the server knows it now: its editor level (the account's GM level when it
	 * is at editor_gm_level, else 0; nothing the client says is used) and whether it is one of this zone's editors.
	 */
	std::optional<Requester> RequesterOf(const SystemAddress& sysAddr, uint32_t zoneId) {
		auto* const user = UserManager::Instance()->GetUser(sysAddr);
		if (!user || zoneId == 0) return std::nullopt;
		const auto editorGmLevel = ConfigNumber("editor_gm_level", CharacterComponent::DEFAULT_EDITOR_GM_LEVEL);
		Requester requester;
		requester.user = user;
		requester.name = user->GetUsername();
		requester.actor.accountId = user->GetAccountID();
		requester.actor.gmLevel = static_cast<uint8_t>(CharacterComponent::EditAttribute(user->GetMaxGMLevel(), editorGmLevel));
		requester.actor.assigned = Database::Get()->IsZoneEditor(zoneId, requester.actor.accountId);
		return requester;
	}

	std::string AccountName(uint32_t accountId) {
		const auto account = Database::Get()->GetAccountById(accountId);
		return account.contains("error") ? "account " + std::to_string(accountId) : account.value("name", std::string());
	}

	void Audit(const Requester& requester, uint32_t zoneId, const std::string& action, const std::string& result) {
		const auto text = "in game, zone " + std::to_string(zoneId) + ", " + action + ": " + result;
		try {
			Database::Get()->InsertAuditLog(requester.actor.accountId, requester.name, AUDIT_ACTION, text, 0, 0);
		} catch (const std::exception& ex) {
			LOG("Failed to write audit log entry: %s", ex.what());
		}
		LOG("[audit] %s: %s %s", requester.name.c_str(), AUDIT_ACTION, text.c_str());
	}

	std::optional<uint32_t> HolderOf(const std::optional<IZoneEditing::ZoneLock>& lock) {
		return lock ? std::optional<uint32_t>(lock->accountId) : std::nullopt;
	}

	// ---- The zone, as the server has it ----

	struct ZoneRef {
		uint32_t zoneId{};
		std::string luzPath; // relative to res/maps/, lowercase
		std::string folder;
	};

	// The zone this world runs; its .luz path comes from the ZoneTable
	std::optional<ZoneRef> ThisZone() {
		const auto zoneId = static_cast<uint32_t>(Game::server->GetZoneID());
		if (zoneId == 0) return std::nullopt;
		const auto* const row = CDZoneTableTable::Query(zoneId);
		if (!row) return std::nullopt;
		auto luz = row->zoneName;
		std::replace(luz.begin(), luz.end(), '\\', '/');
		ZoneRef zone{ zoneId, ZonePublish::Canonical(luz), {} };
		zone.folder = ZonePublish::FolderOf(zone.luzPath);
		if (!zone.luzPath.ends_with(".luz") || ZonePublish::NameProblem(zone.luzPath) || zone.folder.empty()) return std::nullopt;
		return zone;
	}

	std::optional<std::string> ReadAsset(const std::string& name, bool clientOnly = false) {
		auto stream = clientOnly ? Game::assetManager->GetClientFile(name) : Game::assetManager->GetFile(name);
		if (!stream) return std::nullopt;
		return std::string(stream.Bytes());
	}

	// A zone file's scenes and terrain file; nullopt when it can't be read
	std::optional<ZoneFile> ReadZoneHeader(const std::string& bytes) {
		std::istringstream stream(bytes);
		ZoneFile luz;
		try {
			luz.ReadHeader(stream);
		} catch (const std::exception&) {
			return std::nullopt;
		}
		if (stream.fail()) return std::nullopt;
		return luz;
	}

	std::string BaseOf(const std::string& path) {
		const auto slash = path.find_last_of('/');
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}

	std::string TriggersOf(const std::string& scenePath) {
		const auto dot = scenePath.find_last_of('.');
		return (dot == std::string::npos ? scenePath : scenePath.substr(0, dot)) + ".lutriggers";
	}

	// ---- The worker ----

	using Completion = std::function<void()>;
	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<std::function<Completion()>> g_Jobs;
	std::deque<Completion> g_Done;
	bool g_Stopping = false;

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
			std::lock_guard lock(g_Mutex);
			g_Done.push_back(std::move(done));
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

	void SendPushResponse(const SystemAddress& sysAddr, eResult result, uint8_t fileType, int32_t levelFileId, const std::u16string& lockedBy = {}) {
		ClientPackets::SlashPushMapResponse response;
		response.result = static_cast<uint8_t>(result);
		response.fileType = fileType;
		response.levelFileId = levelFileId;
		response.lockedBy = lockedBy;
		response.Send(sysAddr);
	}

	// The zone's instances are replaced by master, as for /reloadworld; every client here is told first
	void StartReload(uint32_t zoneId, const std::string& who) {
		ClientPackets::ImpendingReloadNotify notify;
		notify.Broadcast();
		WorldReloadRequest request;
		request.zoneId = zoneId;
		request.requestedBy = ("zone files, " + who).substr(0, WorldFiles::MAX_BY);
		MasterPackets::SendToMaster(request);
	}
}

namespace ZoneEditor {
	void SetEditing(const SystemAddress& sysAddr, const bool editing) {
		if (editing) g_Editing.insert(sysAddr);
		else g_Editing.erase(sysAddr);
	}

	bool IsEditing(const SystemAddress& sysAddr) {
		return g_Editing.contains(sysAddr);
	}

	bool MaySendZoneLoad(const SystemAddress& sysAddr) {
		return !IsEditing(sysAddr);
	}

	void Forget(const SystemAddress& sysAddr) {
		g_Editing.erase(sysAddr);
		g_Grants.erase(sysAddr);
	}

	void HandleLock(const SystemAddress& sysAddr, const bool lock) {
		const auto zone = ThisZone();
		const auto requester = RequesterOf(sysAddr, zone ? zone->zoneId : 0);
		if (!zone || !requester) return;
		const auto levels = Levels();
		auto held = Database::Get()->GetZoneLock(zone->zoneId);
		auto result = lock ? ZoneEditLock::Take(requester->actor, HolderOf(held), levels) : ZoneEditLock::Release(requester->actor, HolderOf(held), levels);
		std::string detail;
		if (result == eResult::OK && lock) {
			// Someone else got there between the look and the take
			if (!Database::Get()->TakeZoneLock(zone->zoneId, requester->actor.accountId, Now())) {
				held = Database::Get()->GetZoneLock(zone->zoneId);
				result = held && held->accountId == requester->actor.accountId ? eResult::NO_CHANGE : eResult::LOCKED_BY_OTHER;
			}
		} else if (result == eResult::OK) {
			Database::Get()->ReleaseZoneLock(zone->zoneId);
			if (held->accountId != requester->actor.accountId) detail = " (taken away from " + AccountName(held->accountId) + ")";
		}

		const auto now = Database::Get()->GetZoneLock(zone->zoneId);
		ClientPackets::SlashLockMapResponse response;
		response.result = static_cast<uint8_t>(result);
		if (result == eResult::LOCKED_BY_OTHER && now) response.lockedBy.string = GeneralUtils::UTF8ToUTF16(AccountName(now->accountId)).substr(0, 32);
		response.locked = now && now->accountId == requester->actor.accountId ? 1 : 0;
		response.Send(sysAddr);
		Audit(*requester, zone->zoneId, lock ? "lock" : "unlock", "result " + std::to_string(response.result) + detail);
	}

	void HandlePushRequest(const SystemAddress& sysAddr, const uint8_t fileType, const int32_t levelFileId) {
		const auto zone = ThisZone();
		const auto requester = RequesterOf(sysAddr, zone ? zone->zoneId : 0);
		if (!zone || !requester) return;
		g_Grants.erase(sysAddr);
		if (!ZoneFilePack::ValidType(fileType) || Root().empty()) {
			// Nothing a client can be told with the results there are: the nearest is that it may not
			SendPushResponse(sysAddr, eResult::NOT_ASSIGNED, fileType, levelFileId);
			Audit(*requester, zone->zoneId, "push request", Root().empty() ? "refused (edited zone files are turned off)" : "refused (unknown file type)");
			return;
		}
		const auto held = Database::Get()->GetZoneLock(zone->zoneId);
		const auto result = ZoneEditLock::ChangeOrWhyNot(requester->actor, HolderOf(held), Levels());
		std::u16string lockedBy;
		if (result == eResult::LOCKED_BY_OTHER) lockedBy = held ? GeneralUtils::UTF8ToUTF16(AccountName(held->accountId)) : std::u16string(NOBODY);
		if (result == eResult::OK) g_Grants[sysAddr] = { fileType, levelFileId, Now() + PUSH_GRANT_SECONDS };
		SendPushResponse(sysAddr, result, fileType, levelFileId, lockedBy);
		Audit(*requester, zone->zoneId, "push request (file type " + std::to_string(fileType) + ")", "result " + std::to_string(static_cast<uint8_t>(result)));
	}

	void HandlePush(const SystemAddress& sysAddr, const uint8_t fileType, const int32_t levelFileId, std::string packedFiles) {
		const auto zone = ThisZone();
		const auto requester = RequesterOf(sysAddr, zone ? zone->zoneId : 0);
		if (!zone || !requester) return;
		const auto refuse = [&](const std::string& why) {
			Audit(*requester, zone->zoneId, "push", "refused (" + why + "); nothing was written");
			ChatPackets::SendSystemMessage(sysAddr, u"The map files were not taken: " + GeneralUtils::UTF8ToUTF16(why));
		};

		// Only what was asked for and allowed just before, and everything is looked at again now
		const auto grant = g_Grants.find(sysAddr);
		const bool granted = grant != g_Grants.end() && grant->second.fileType == fileType && grant->second.levelFileId == levelFileId && grant->second.until >= Now();
		if (grant != g_Grants.end()) g_Grants.erase(grant);
		if (!granted) return refuse("the files were not asked for first");
		const auto root = Root();
		if (root.empty()) return refuse("edited zone files are turned off");
		const auto result = ZoneEditLock::Change(requester->actor, HolderOf(Database::Get()->GetZoneLock(zone->zoneId)), Levels());
		if (result != eResult::OK) return refuse("result " + std::to_string(static_cast<uint8_t>(result)));
		if (g_Changing) return refuse("this zone's files are being changed right now");
		const auto limits = Limits();
		if (packedFiles.size() > limits.maxTotalBytes) return refuse("larger than this server takes");

		ZoneFilePush::Job job;
		job.zoneId = zone->zoneId;
		job.luzPath = zone->luzPath;
		job.fileType = static_cast<eFileType>(fileType);
		job.packedFiles = std::move(packedFiles);
		job.limits = limits;
		job.root = root;
		job.id = "w" + std::to_string(Game::server->GetInstanceID()) + "-" + std::to_string(Now()) + "-" + std::to_string(++g_NextChange);
		job.who = requester->name;
		job.now = Now();
		job.keepBackups = ConfigNumber("zone_publish_max_backups", 20);
		// The client's own files say which scenes the zone has, and which files a set may leave out
		job.clientLuz = ReadAsset("maps/" + zone->luzPath, true);
		if (const auto luz = job.clientLuz ? ReadZoneHeader(*job.clientLuz) : std::nullopt) {
			for (const auto& scene : luz->scenes) {
				const auto path = zone->folder + ZonePublish::Canonical(scene.filename);
				job.clientScenes.insert(path);
				if (Game::assetManager->HasClientFile("maps/" + path)) job.clientFiles.insert(path);
			}
			const auto terrain = zone->folder + ZonePublish::Canonical(luz->zoneRawPath);
			if (Game::assetManager->HasClientFile("maps/" + terrain)) job.clientFiles.insert(terrain);
		}

		g_Changing = true;
		const auto who = *requester;
		const auto zoneId = zone->zoneId;
		Submit([job = std::move(job), who, zoneId, sysAddr, fileType]() -> Completion {
			const auto result = ZoneFilePush::Run(job);
			return [result, who, zoneId, sysAddr, fileType, id = job.id] {
				g_Changing = false;
				if (!result.applied) {
					Audit(who, zoneId, "push (file type " + std::to_string(fileType) + ")", "refused (" + result.problem + "); nothing was written");
					ChatPackets::SendSystemMessage(sysAddr, u"The map files were not taken: " + GeneralUtils::UTF8ToUTF16(result.problem));
					return;
				}
				Audit(who, zoneId, "push (file type " + std::to_string(fileType) + ")", "applied as " + id + " (" + std::to_string(result.files) + " files" +
					(result.backupId ? ", the set before is backup " + *result.backupId : "") + "), reload started");
				StartReload(zoneId, who.name);
			};
		});
	}

	void HandlePull(const SystemAddress& sysAddr, const uint8_t fileType, const int32_t levelFileId) {
		const auto zone = ThisZone();
		const auto requester = RequesterOf(sysAddr, zone ? zone->zoneId : 0);
		if (!zone || !requester) return;
		const auto refuse = [&](const std::string& why) {
			Audit(*requester, zone->zoneId, "pull", "refused (" + why + ")");
			ChatPackets::SendSystemMessage(sysAddr, u"The map files were not sent: " + GeneralUtils::UTF8ToUTF16(why));
		};
		if (!ZoneFilePack::ValidType(fileType)) return refuse("unknown file type");
		const auto result = ZoneEditLock::Read(requester->actor, Levels());
		if (result != eResult::OK) return refuse("result " + std::to_string(static_cast<uint8_t>(result)));

		// The files as this server reads them now (an edited set, else the client's), read here on the main thread
		const auto type = static_cast<eFileType>(fileType);
		const auto luzBytes = ReadAsset("maps/" + zone->luzPath);
		const auto luz = luzBytes ? ReadZoneHeader(*luzBytes) : std::nullopt;
		if (!luz) return refuse("the zone file can't be read");
		const auto limit = Limits().maxTotalBytes;
		uint64_t total = 0;
		bool tooLarge = false;
		const auto read = [&](const std::string& path) -> std::optional<ZoneFilePack::Entry> {
			auto bytes = ReadAsset("maps/" + path);
			if (!bytes) return std::nullopt;
			total += bytes->size();
			tooLarge = tooLarge || total > limit;
			return ZoneFilePack::Entry{ BaseOf(path), std::move(*bytes) };
		};
		auto files = std::make_shared<ZoneFilePack::Files>();
		const bool whole = type == eFileType::WHOLE_MAP;
		if (whole || type == eFileType::ZONE) files->zone = ZoneFilePack::Entry{ BaseOf(zone->luzPath), *luzBytes };
		if (whole || type == eFileType::TERRAIN) files->terrain = read(zone->folder + ZonePublish::Canonical(luz->zoneRawPath));
		if (whole || type == eFileType::SCENE || type == eFileType::TRIGGERS) {
			for (const auto& scene : luz->scenes) {
				if (!whole && scene.id != static_cast<uint32_t>(levelFileId)) continue;
				if (ZonePublish::NameProblem(scene.filename)) continue;
				const auto path = zone->folder + ZonePublish::Canonical(scene.filename);
				if (whole || type == eFileType::SCENE) {
					if (auto entry = read(path)) files->scenes.push_back(std::move(*entry));
				}
				if (whole || type == eFileType::TRIGGERS) {
					if (auto entry = read(TriggersOf(path))) files->triggers.push_back(std::move(*entry));
				}
				if (!whole) break;
			}
		}
		if (tooLarge) return refuse("larger than this server sends");
		const bool found = type == eFileType::ZONE ? files->zone.has_value() : type == eFileType::TERRAIN ? files->terrain.has_value() :
			type == eFileType::SCENE ? !files->scenes.empty() : type == eFileType::TRIGGERS ? !files->triggers.empty() : files->zone && files->terrain;
		if (!found) return refuse("the zone has no such file");

		const auto who = *requester;
		const auto zoneId = zone->zoneId;
		Submit([files, type, who, zoneId, sysAddr, fileType, levelFileId]() -> Completion {
			auto packed = std::make_shared<std::string>(ZoneFilePack::Pack(type, *files));
			const auto count = files->Count();
			return [packed, count, who, zoneId, sysAddr, fileType, levelFileId] {
				// Gone meanwhile: nobody to send it to
				if (!UserManager::Instance()->GetUser(sysAddr)) return;
				ClientPackets::SlashPullMapResponse response;
				response.fileType = fileType;
				response.levelFileId = levelFileId;
				response.packedFiles = std::move(*packed);
				response.Send(sysAddr);
				Audit(who, zoneId, "pull (file type " + std::to_string(fileType) + ")", "sent " + std::to_string(count) + " file(s)");
			};
		});
	}

	void HandleReload(const SystemAddress& sysAddr) {
		const auto zone = ThisZone();
		const auto requester = RequesterOf(sysAddr, zone ? zone->zoneId : 0);
		if (!zone || !requester) return;
		const auto result = ZoneEditLock::Read(requester->actor, Levels());
		if (result != eResult::OK) {
			Audit(*requester, zone->zoneId, "reload", "refused (result " + std::to_string(static_cast<uint8_t>(result)) + ")");
			ChatPackets::SendSystemMessage(sysAddr, u"You may not reload this map.");
			return;
		}
		Audit(*requester, zone->zoneId, "reload", "reload started");
		StartReload(zone->zoneId, requester->name);
	}

	void Update() {
		std::deque<Completion> done;
		{
			std::lock_guard lock(g_Mutex);
			if (g_Done.empty()) return;
			done.swap(g_Done);
		}
		for (auto& completion : done) {
			try {
				if (completion) completion();
			} catch (const std::exception& ex) {
				LOG("Zone editor: finishing a request failed: %s", ex.what());
			}
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
}
