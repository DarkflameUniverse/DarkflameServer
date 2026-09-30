#include "CharacterTools.h"
#include "GameText.h"

#include <ctime>

#include "RouteUtils.h"
#include "CharacterXml.h"
#include "ObjectIDManager.h"
#include "Scheduler.h"
#include "Background.h"
#include "PlayerActions.h"
#include "master/PlayerAction.h"
#include "WSRoutes.h"
#include "ClientAssets.h"
#include "DashboardRoutes.h"
#include "tinyxml2.h"
#include "DashboardAuthService.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "ZCompression.h"
#include "eHTTPMethod.h"
#include "eServerDisconnectIdentifiers.h"

using namespace RouteUtils;

namespace {
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;

	int64_t Setting(const std::string& key, int64_t fallback) {
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	std::string Compress(const std::string& data) {
		std::string out(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(data.size())), '\0');
		const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(data.data()), static_cast<uint32_t>(data.size()),
			reinterpret_cast<uint8_t*>(out.data()), static_cast<uint32_t>(out.size()));
		out.resize(size > 0 ? static_cast<size_t>(size) : 0);
		return out;
	}

	std::optional<std::string> Decompress(const ICharacterSnapshots::CharacterSnapshot& snapshot) {
		std::string out(snapshot.size, '\0');
		int32_t error = 0;
		const auto size = ZCompression::Decompress(reinterpret_cast<const uint8_t*>(snapshot.compressed.data()), static_cast<uint32_t>(snapshot.compressed.size()),
			reinterpret_cast<uint8_t*>(out.data()), snapshot.size, error);
		if (size != static_cast<int32_t>(snapshot.size)) return std::nullopt;
		return out;
	}

	ICharacterSnapshots::CharacterSnapshot MakeSnapshot(LWOOBJID characterId, const std::string& xml, const std::string& reason, const std::string& actor) {
		return { 0, characterId, static_cast<int64_t>(std::time(nullptr)), reason, actor, static_cast<uint32_t>(xml.size()),
			DashboardAuthService::Sha256Hex(xml), Compress(xml) };
	}

	bool LotExists(LOT lot) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT 1 FROM Objects WHERE id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int32_t>(lot));
		return !stmt.execQuery().eof();
	}

	// Persistent object ids for items the editor adds, from the world servers' ObjectIDManager
	int64_t NewItemId() {
		return static_cast<int64_t>(ObjectIDManager::GetPersistentID());
	}

	// Add item names so the page can show them
	void Name(nlohmann::json& summary) {
		if (!summary.is_object()) return;
		for (auto& inventory : summary["inventories"]) for (auto& item : inventory["items"]) item["name"] = ClientAssets::ItemName(item["lot"].get<LOT>());
	}


	nlohmann::json WithZones(nlohmann::json rows) {
		for (auto& row : rows) row["zone_name"] = GameText::ZoneName(row.value("zone_id", 0u));
		return rows;
	}

	nlohmann::json Transfers(const std::vector<LWOOBJID>& ids) {
		auto rows = Database::Get()->GetTransfersForCharacters(ids, 0, 50)["data"];
		for (auto& row : rows) if (row.value("lot", 0) > 0) row["name"] = ClientAssets::ItemName(row["lot"].get<LOT>());
		return rows;
	}

	// Pets from the character's saved data: <pet><p id l (LOT) m (moderation: 2 approved) n (name)/></pet>
	nlohmann::json Pets(const std::string& xml) {
		nlohmann::json pets = nlohmann::json::array();
		for (const auto& pet : CharacterXml::Pets(xml)) {
			std::string name = pet.name;
			// pet_names is the current word (the save only updates when the owner plays): 1 waiting, 2 approved,
			// no row after a rejection
			std::string status = "none", reason;
			const auto current = Database::Get()->GetPetNameInfo(pet.id);
			if (current) {
				name = current->petName;
				status = current->approvalStatus == 2 ? "approved" : "waiting";
			} else if (!name.empty()) {
				status = "rejected";
			}
			const auto decisions = Database::Get()->GetModerationDecisions("pet_name", pet.id, 1);
			if (status == "rejected" && !decisions.empty() && !decisions[0]["approved"].get<bool>()) reason = decisions[0]["reason"];
			pets.push_back({ {"id", std::to_string(pet.id)}, {"lot", pet.lot}, {"kind", ClientAssets::ItemName(pet.lot)},
				{"name", name}, {"approved", status == "approved"}, {"status", status}, {"reason", reason} });
		}
		return pets;
	}

	// A character's name requests: the one waiting (if any), then earlier decisions with the moderator's reason
	nlohmann::json NameRequests(LWOOBJID charId, const ICharInfo::Info& info) {
		nlohmann::json rows = nlohmann::json::array();
		if (!info.pendingName.empty()) rows.push_back({ {"character_id", std::to_string(charId)}, {"character_name", info.name}, {"name", info.pendingName}, {"status", "waiting"}, {"reason", ""}, {"time", 0} });
		else if (info.needsRename) rows.push_back({ {"character_id", std::to_string(charId)}, {"character_name", info.name}, {"name", ""}, {"status", "rename_needed"}, {"reason", ""}, {"time", 0} });
		for (const auto& d : Database::Get()->GetModerationDecisions("name", charId, 20)) {
			rows.push_back({ {"character_id", std::to_string(charId)}, {"character_name", info.name}, {"name", d["subject"]},
				{"status", d["approved"].get<bool>() ? "approved" : "rejected"}, {"reason", d["reason"]}, {"time", d["time"]} });
		}
		return rows;
	}

	// Recent chat by or to a character (or by an account), for the Related card; private chat only with chat_private
	nlohmann::json RecentChat(const HTTPContext& context, LWOOBJID characterId, uint32_t accountId) {
		IChatLog::ChatQuery q;
		q.characterId = characterId;
		q.accountId = accountId;
		q.includePrivate = Can(context, "chat_private");
		q.includeWhispers = Can(context, "chat_dms");
		q.newestFirst = true;
		q.limit = 100;
		nlohmann::json rows = nlohmann::json::array();
		for (const auto& m : Database::Get()->GetChatMessages(q)) {
			rows.push_back({ {"id", m.id}, {"time", m.time}, {"channel", m.channel}, {"sender_id", std::to_string(m.senderId)}, {"sender_name", m.senderName},
				{"recipient_id", std::to_string(m.recipientId)}, {"recipient_name", m.recipientName}, {"zone_id", m.zoneId}, {"message", m.message}, {"blocked", m.blocked} });
		}
		return rows;
	}

	std::optional<LWOOBJID> SnapshotCharacter(const HTTPContext& context, uint64_t snapshotId, HTTPReply& reply, std::optional<ICharacterSnapshots::CharacterSnapshot>& out) {
		out = Database::Get()->GetCharacterSnapshot(snapshotId);
		if (!out) { JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Snapshot not found"); return std::nullopt; }
		return out->characterId;
	}

	void TakeDailySnapshots(Scheduler::RunPtr run) {
		const auto keep = static_cast<uint32_t>(std::max<int64_t>(Setting("snapshot_keep", 10), 1));
		const auto days = std::max<int64_t>(Setting("snapshot_days", 90), 1);
		const bool queued = Background::Run("character_snapshots", [keep, days](GameDatabase& db) -> nlohmann::json {
			const auto latest = db.GetLatestSnapshotHashes();
			uint32_t taken = 0, unchanged = 0;
			db.ForEachCharacterXml([&](LWOOBJID characterId, const std::string& xml) {
				if (xml.empty()) return;
				const auto hash = DashboardAuthService::Sha256Hex(xml);
				const auto it = latest.find(characterId);
				if (it != latest.end() && it->second == hash) { unchanged++; return; }
				auto snapshot = MakeSnapshot(characterId, xml, "daily", "");
				snapshot.hash = hash;
				db.InsertCharacterSnapshot(snapshot);
				taken++;
			});
			const auto pruned = db.PruneCharacterSnapshots(static_cast<int64_t>(std::time(nullptr)) - days * DAY_SECONDS, keep);
			return { {"taken", taken}, {"unchanged", unchanged}, {"pruned", pruned} };
		}, [run](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			run->Log(std::to_string(result["unchanged"].get<uint32_t>()) + " characters unchanged since their last snapshot");
			run->Finish(true, std::to_string(result["taken"].get<uint32_t>()) + " snapshot(s) taken, " + std::to_string(result["pruned"].get<uint32_t>()) + " old one(s) removed");
		});
		if (!queued) run->Finish(false, "Snapshots are already being taken");
	}
}

namespace {
	// Once the owner is disconnected: apply the change, keep a snapshot of the version before it, and audit it
	PlayerActions::Outcome Write(LWOOBJID characterId, const std::string& actor, uint32_t actorId, const std::string& reason, const PlayerActionResult& result,
		const std::function<std::optional<std::string>(const std::string& current, std::string& error)>& change, const std::function<std::string(const std::string& before, const std::string& after)>& describe) {
		if (result.timedOut && result.affected == 0) {
			return PlayerActions::Outcome{ false, "Not every world server responded, so the character may still be online. Nothing was changed; try again." };
		}
		const auto current = Database::Get()->GetCharacterXml(characterId);
		std::string error;
		const auto updated = change(current, error);
		if (!updated) return PlayerActions::Outcome{ false, error.empty() ? "Nothing to change" : error };
		if (!current.empty()) Database::Get()->InsertCharacterSnapshot(MakeSnapshot(characterId, current, "before " + reason, actor));
		Database::Get()->UpdateCharacterXml(characterId, *updated);
		const auto info = Database::Get()->GetCharacterInfo(characterId);
		const auto name = info ? info->name : std::to_string(characterId);
		const auto what = describe ? describe(current, *updated) : reason;
		Database::Get()->InsertAuditLog(actorId, actor, "edit_character", name + ": " + what + (info ? OwnAccountNote(actorId, info->accountId) : ""), info ? info->accountId : 0, characterId);
		BroadcastTableChanged("characters", std::to_string(characterId));
		return PlayerActions::Outcome{ true, "Saved " + name + (result.affected ? " (they were disconnected)" : "") + ". The previous version is in the history." };
	}
}

uint32_t WriteCharacterXml(LWOOBJID characterId, uint32_t ownerAccountId, const std::string& actor, uint32_t actorId, const std::string& reason,
	std::function<std::optional<std::string>(const std::string& current, std::string& error)> change, std::function<std::string(const std::string& before, const std::string& after)> describe,
	std::function<void(const PlayerActions::Outcome& outcome)> done) {
	// Disconnect the owner first so their world saves and lets go of the character; otherwise the world's next save
	// would overwrite this change
	PlayerActionRequest request;
	request.action = ePlayerAction::KICK_ACCOUNT;
	request.accountId = ownerAccountId;
	request.disconnectReason = static_cast<uint32_t>(eServerDisconnectIdentifiers::KICK);
	return PlayerActions::Request(request, actorId, [=](const PlayerActionResult& result) {
		const auto outcome = Write(characterId, actor, actorId, reason, result, change, describe);
		if (done) done(outcome);
		return outcome;
	});
}

std::optional<std::string> SnapshotXml(const ICharacterSnapshots::CharacterSnapshot& snapshot) {
	return Decompress(snapshot);
}

void RegisterCharacterTasks() {
	Scheduler::Register({ "character_snapshots", "Character snapshots",
		"Saves a copy of every character whose data changed since its last snapshot, so lost or broken characters can be "
		"restored. Keeps snapshots for snapshot_days, and always each character's newest snapshot_keep.",
		"0 4 * * *", TakeDailySnapshots, 6 * 60 * 60 });
}

void RegisterCharacterToolRoutes() {
	Route(eHTTPMethod::GET, "/api/characters/:id/summary", 0, "Coins, U-score, level and inventories from a character's saved data (with item names)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
			auto summary = CharacterXml::Summary(Database::Get()->GetCharacterXml(*charId));
			if (summary.is_null()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "This character has no readable saved data");
			Name(summary);
			JsonSuccess(reply, { {"summary", summary} });
		});

	Route(eHTTPMethod::GET, "/api/characters/:id/related", 0,
		"Everything tied to a character: properties, pets, friends, trades and mail, bug reports and economy flags (each only with the permission to see it)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
			const bool own = info->accountId == context.accountId;
			nlohmann::json related = nlohmann::json::object();
			if ((own && Can(context, "own_properties")) || Can(context, "properties_view")) related["properties"] = WithZones(Database::Get()->GetPropertiesOwnedBy(*charId));
			related["pets"] = Pets(Database::Get()->GetCharacterXml(*charId));
			if ((own && Can(context, "own_characters")) || Can(context, "moderate_names")) related["names"] = NameRequests(*charId, *info);
			related["friends"] = Database::Get()->GetFriendsOf(*charId);
			if ((own && Can(context, "own_history")) || Can(context, "reports_view")) related["transfers"] = Transfers({ *charId });
			if (Can(context, "bug_reports_view")) related["bug_reports"] = Database::Get()->GetBugReportsBy(*charId, 50);
			if (Can(context, "reports_view")) related["flags"] = Database::Get()->GetEconomyFlagsFor(*charId, 50);
			if (Can(context, "chat_view")) related["chat"] = RecentChat(context, *charId, 0);
			related["logs"] = { {"activity", Can(context, "logs_activity")}, {"commands", Can(context, "logs_command")} };
			JsonSuccess(reply, { {"related", related} });
		});

	Route(eHTTPMethod::GET, "/api/accounts/:id/related", 0,
		"Everything tied to an account: its characters' properties, trades and mail, bug reports and economy flags, cheat detections and "
		"dashboard actions about it (each only with the permission to see it)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const bool own = *accountId == context.accountId;
			if (!own && !Can(context, "accounts_view")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own account");
			const auto ids = Database::Get()->GetAccountCharacterIds(*accountId);
			std::map<LWOOBJID, std::string> names;
			std::map<LWOOBJID, ICharInfo::Info> infos;
			for (const auto id : ids) if (const auto info = Database::Get()->GetCharacterInfo(id)) { names[id] = info->name; infos[id] = *info; }
			nlohmann::json related = nlohmann::json::object();
			nlohmann::json pets = nlohmann::json::array();
			for (const auto id : ids) for (auto row : Pets(Database::Get()->GetCharacterXml(id))) { row["owner_id"] = std::to_string(id); row["owner_name"] = names[id]; pets.push_back(std::move(row)); }
			if ((own && Can(context, "own_characters")) || Can(context, "characters_view")) related["pets"] = pets;
			if ((own && Can(context, "own_characters")) || Can(context, "moderate_names")) {
				nlohmann::json requests = nlohmann::json::array();
				for (const auto& [id, info] : infos) for (auto row : NameRequests(id, info)) requests.push_back(std::move(row));
				related["names"] = requests;
			}
			if ((own && Can(context, "own_properties")) || Can(context, "properties_view")) {
				nlohmann::json properties = nlohmann::json::array();
				for (const auto id : ids) for (auto row : WithZones(Database::Get()->GetPropertiesOwnedBy(id))) {
					row["owner_id"] = std::to_string(id);
					row["owner_name"] = names[id];
					properties.push_back(std::move(row));
				}
				related["properties"] = properties;
			}
			if ((own && Can(context, "own_history")) || Can(context, "reports_view")) related["transfers"] = Transfers(ids);
			if (Can(context, "bug_reports_view")) {
				nlohmann::json reports = nlohmann::json::array();
				for (const auto id : ids) for (auto row : Database::Get()->GetBugReportsBy(id, 50)) { row["character_id"] = std::to_string(id); row["character_name"] = names[id]; reports.push_back(std::move(row)); }
				related["bug_reports"] = reports;
			}
			if (Can(context, "reports_view")) {
				nlohmann::json flags = nlohmann::json::array();
				for (const auto id : ids) for (auto row : Database::Get()->GetEconomyFlagsFor(id, 50)) { row["character_id"] = std::to_string(id); row["character_name"] = names[id]; flags.push_back(std::move(row)); }
				related["flags"] = flags;
			}
			if (Can(context, "chat_view")) related["chat"] = RecentChat(context, 0, *accountId);
			if (Can(context, "accounts_notes")) related["cheats"] = Database::Get()->GetCheatDetectionsFor(*accountId, 100);
			if (Can(context, "logs_audit")) related["audit"] = Database::Get()->GetAuditAbout(*accountId, 100);
			JsonSuccess(reply, { {"related", related} });
		});

	Route(eHTTPMethod::POST, "/api/characters/:id/edit", Perm("characters_edit"),
		"Edit a character (the owner is disconnected first, and a snapshot is kept). Body: {coins, uscore, level, counts: {itemId: count}, "
		"remove: [itemId], add: [{lot, count, inventory}]}. Returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto body = ParseBody(context);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (!AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::ITEMS)) return;
			if (body->contains("add") && (*body)["add"].is_array()) {
				for (const auto& add : (*body)["add"]) {
					if (!LotExists(add.value("lot", 0))) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown item LOT " + std::to_string(add.value("lot", 0)));
				}
			}
			// Check the edit against the current data now, so mistakes are reported straight away
			std::string error;
			if (!CharacterXml::Apply(Database::Get()->GetCharacterXml(*charId), *body, [] { return int64_t{ 1 }; }, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
			const auto edit = *body;
			const auto requestId = WriteCharacterXml(*charId, info->accountId, context.authenticatedUser, context.accountId, "edit",
				[edit](const std::string& current, std::string& error) { return CharacterXml::Apply(current, edit, NewItemId, error); },
				[edit](const std::string&, const std::string&) {
					std::vector<std::string> parts;
					for (const auto* key : { "coins", "uscore", "level" }) if (edit.contains(key) && !edit[key].is_null()) parts.push_back(std::string(key) + " = " + edit[key].dump());
					if (edit.contains("counts")) parts.push_back(std::to_string(edit["counts"].size()) + " count(s) changed");
					if (edit.contains("remove")) parts.push_back(std::to_string(edit["remove"].size()) + " item(s) removed");
					if (edit.contains("add")) for (const auto& add : edit["add"]) parts.push_back("added " + std::to_string(add.value("count", 1)) + "x " + ClientAssets::ItemName(add.value("lot", 0)));
					std::string text;
					for (const auto& part : parts) text += (text.empty() ? "" : ", ") + part;
					return text.empty() ? std::string("no changes") : text;
				});
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::GET, "/api/characters/:id/snapshots", Perm("characters_history"), "Saved earlier versions of a character, newest first",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			if (!charId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			nlohmann::json list = nlohmann::json::array();
			for (const auto& s : Database::Get()->GetCharacterSnapshots(*charId)) {
				list.push_back({ {"id", s.id}, {"taken_at", s.takenAt}, {"reason", s.reason}, {"actor", s.actor}, {"size", s.size} });
			}
			JsonSuccess(reply, { {"snapshots", list} });
		});

	Route(eHTTPMethod::GET, "/api/snapshots/:id", Perm("characters_history"), "One snapshot's summary (coins, level, inventories), to compare with the character now",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 2);
			std::optional<ICharacterSnapshots::CharacterSnapshot> snapshot;
			if (!id || !SnapshotCharacter(context, *id, reply, snapshot)) return;
			const auto xml = Decompress(*snapshot);
			auto summary = xml ? CharacterXml::Summary(*xml) : nullptr;
			if (summary.is_null()) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "This snapshot can't be read");
			Name(summary);
			JsonSuccess(reply, { {"summary", summary}, {"taken_at", snapshot->takenAt}, {"reason", snapshot->reason}, {"character_id", std::to_string(snapshot->characterId)} });
		});

	Route(eHTTPMethod::GET, "/api/snapshots/:id/xml", Perm("characters_history"), "Download a snapshot's character XML",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 2);
			std::optional<ICharacterSnapshots::CharacterSnapshot> snapshot;
			if (!id || !SnapshotCharacter(context, *id, reply, snapshot)) return;
			const auto xml = Decompress(*snapshot);
			if (!xml) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "This snapshot can't be read");
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			reply.message = *xml;
			reply.headers.push_back("Content-Disposition: attachment; filename=\"character-" + std::to_string(snapshot->characterId) + "-" + std::to_string(snapshot->id) + ".xml\"");
		});

	Route(eHTTPMethod::POST, "/api/snapshots/:id/restore", Perm("characters_edit"),
		"Put a character back to a snapshot (the owner is disconnected first; the current version is kept as a snapshot). Returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 2);
			std::optional<ICharacterSnapshots::CharacterSnapshot> snapshot;
			if (!id || !SnapshotCharacter(context, *id, reply, snapshot)) return;
			const auto info = Database::Get()->GetCharacterInfo(snapshot->characterId);
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That character no longer exists");
			if (!AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::ITEMS)) return;
			const auto xml = Decompress(*snapshot);
			if (!xml) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "This snapshot can't be read");
			const auto takenAt = snapshot->takenAt;
			const auto restored = *xml;
			const auto requestId = WriteCharacterXml(snapshot->characterId, info->accountId, context.authenticatedUser, context.accountId, "restore",
				[restored](const std::string&, std::string&) { return std::optional<std::string>(restored); },
				[takenAt](const std::string&, const std::string&) {
					char when[32];
					const std::time_t time = takenAt;
					std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M UTC", std::gmtime(&time));
					return std::string("restored the snapshot from ") + when;
				});
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
