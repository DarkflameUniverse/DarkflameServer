#include "CharacterRestore.h"

#include <ctime>
#include <map>
#include <set>

#include "RouteUtils.h"
#include "InventoryRestore.h"
#include "CharacterTools.h"
#include "PlayerActions.h"
#include "Background.h"
#include "ClientAssets.h"
#include "WSRoutes.h"
#include "Database.h"
#include "GameDatabase.h"
#include "MailInfo.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	// Mails one restore may send; the game shows a mailbox 20 at a time, the rest as it empties
	constexpr size_t MAX_MAILS = 100;
	constexpr size_t MAX_NOTE = 400;

	// Worker thread: the snapshot compared with the character now, with everything that holds its lost objects
	InventoryRestore::Diff Scan(GameDatabase& db, LWOOBJID characterId, const std::string& snapshotXml) {
		const auto then = InventoryRestore::Held(snapshotXml);
		const auto now = InventoryRestore::Held(db.GetCharacterXml(characterId));
		std::set<LWOOBJID> lostIds;
		for (const auto& item : then) if (item.id != LWOOBJID_EMPTY) lostIds.insert(item.id);
		for (const auto& item : now) lostIds.erase(item.id);

		std::map<LOT, uint64_t> mailed;
		std::set<LWOOBJID> mailedIds;
		std::map<LWOOBJID, uint64_t> elsewhere;
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) {
			if (attachment.receiverId == characterId) {
				mailed[attachment.lot] += attachment.count;
				if (attachment.itemId != LWOOBJID_EMPTY) mailedIds.insert(attachment.itemId);
			} else if (lostIds.contains(attachment.itemId)) {
				elsewhere[attachment.itemId] += attachment.count;
			}
		});
		if (!lostIds.empty()) {
			db.ForEachCharacterXml([&](LWOOBJID otherId, const std::string& xml) {
				if (otherId == characterId) return;
				EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
					if (InventoryRestore::CountsAsHeld(item) && lostIds.contains(item.id)) elsewhere[item.id] += item.count;
				});
			});
		}
		return InventoryRestore::Compare(then, now, mailed, mailedIds, elsewhere);
	}

	nlohmann::json DiffJson(const InventoryRestore::Diff& diff) {
		nlohmann::json changes = nlohmann::json::array();
		for (const auto& change : diff.changes) changes.push_back({ {"inventory", change.inventory}, {"lot", change.lot}, {"then", change.then}, {"now", change.now} });
		nlohmann::json missing = nlohmann::json::array();
		for (const auto& entry : diff.missing) {
			missing.push_back({ {"lot", entry.lot}, {"then", entry.then}, {"now", entry.now}, {"mailed", entry.mailed}, {"elsewhere", entry.elsewhere},
				{"restorable", entry.restorable}, {"original_ids", entry.lost.size()} });
		}
		return { {"changes", changes}, {"missing", missing} };
	}

	// Main thread: item names and the item sets the missing items belong to
	nlohmann::json Named(nlohmann::json diff) {
		for (auto& change : diff["changes"]) change["name"] = ClientAssets::ItemName(change["lot"].get<LOT>());
		std::map<int, nlohmann::json> sets;
		for (auto& entry : diff["missing"]) {
			const auto lot = entry["lot"].get<LOT>();
			entry["name"] = ClientAssets::ItemName(lot);
			const auto& set = ClientAssets::ItemInfo(lot)["set"];
			entry["set"] = set.is_object() ? set["id"] : nlohmann::json(nullptr);
			if (!set.is_object()) continue;
			auto& group = sets[set["id"].get<int>()];
			if (group.is_null()) group = { {"id", set["id"]}, {"name", set["name"]}, {"lots", nlohmann::json::array()} };
			group["lots"].push_back(lot);
		}
		diff["sets"] = nlohmann::json::array();
		for (auto& [id, set] : sets) diff["sets"].push_back(std::move(set));
		return diff;
	}

	// The snapshot and its character, or an error reply
	struct Target {
		ICharacterSnapshots::CharacterSnapshot snapshot;
		ICharInfo::Info character;
		std::string xml;
	};

	std::optional<Target> FindTarget(const HTTPContext& context, HTTPReply& reply) {
		const auto charId = PathId<LWOOBJID>(context.path, 2);
		const auto snapshotId = PathId<uint64_t>(context.path, 4);
		const auto snapshot = snapshotId ? Database::Get()->GetCharacterSnapshot(*snapshotId) : std::nullopt;
		if (!charId || !snapshot || snapshot->characterId != *charId) { JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Snapshot not found"); return std::nullopt; }
		const auto character = Database::Get()->GetCharacterInfo(*charId);
		if (!character) { JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found"); return std::nullopt; }
		auto xml = SnapshotXml(*snapshot);
		if (!xml) { JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "This snapshot can't be read"); return std::nullopt; }
		return Target{ *snapshot, *character, std::move(*xml) };
	}

	std::string TaskName(LWOOBJID characterId) {
		return "restore-items:" + std::to_string(characterId);
	}
}

void RegisterCharacterRestoreRoutes() {
	Route(eHTTPMethod::GET, "/api/characters/:id/restore/:snapshot", Perm("items_restore"),
		"What a character had in a snapshot and not now. Runs in the background: returns {requestId}; the result's data has "
		"{changes: [{inventory, lot, name, then, now}], missing: [{lot, name, then, now, mailed, elsewhere, restorable, set}], sets: [{id, name, lots}]}",
		[](HTTPReply& reply, const HTTPContext& context) {
			// A snapshot's contents are character history too, whatever items_restore is set to
			if (!Can(context, "characters_history")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Seeing snapshots also needs the characters_history permission");
			auto target = FindTarget(context, reply);
			if (!target) return;
			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(10));
			const auto characterId = target->character.id;
			const auto takenAt = target->snapshot.takenAt;
			const bool queued = Background::Run(TaskName(characterId) + ":" + std::to_string(requestId),
				[characterId, xml = std::move(target->xml)](GameDatabase& db) { return DiffJson(Scan(db, characterId, xml)); },
				[requestId, takenAt](nlohmann::json diff, const std::string& error) {
					if (!error.empty()) return PlayerActions::Finish(requestId, { false, "Comparing failed: " + error });
					auto named = Named(std::move(diff));
					named["taken_at"] = takenAt;
					PlayerActions::Finish(requestId, { true, "", named });
				});
			if (!queued) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Already comparing");
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::POST, "/api/characters/:id/restore/:snapshot", Perm("items_restore"),
		"Mail a character items it had in a snapshot and has lost since. Body: {items: [{lot, count}], note}. Only what is still missing "
		"is sent (see GET); lost objects nobody holds go back with their original IDs. Runs in the background: returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Can(context, "characters_history")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Restoring from snapshots also needs the characters_history permission");
			const auto body = ParseBody(context);
			if (!body || !(*body)["items"].is_array() || (*body)["items"].empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick the items to give back");
			std::map<LOT, uint64_t> wanted;
			for (const auto& item : (*body)["items"]) {
				const LOT lot = item.value("lot", 0);
				const int64_t count = item.value("count", 0);
				if (lot <= 0 || count < 1) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Each item needs a LOT and a count of at least 1");
				wanted[lot] += static_cast<uint64_t>(count);
			}
			const std::string note = body->value("note", "");
			if (note.size() > MAX_NOTE) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The note is too long");
			auto target = FindTarget(context, reply);
			if (!target) return;
			if (!AuthorizeAccountAction(context, target->character.accountId, reply, eAccountAction::ITEMS)) return;

			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(10));
			const auto character = target->character;
			const auto takenAt = target->snapshot.takenAt;
			MailInfo mail;
			mail.senderId = LWOOBJID_EMPTY;
			mail.senderUsername = "[GM] " + context.authenticatedUser;
			mail.receiverId = character.id;
			mail.recipient = character.name;
			mail.subject = "Items returned to you";
			mail.body = note.empty() ? "A moderator sent back items you lost." : note;
			mail.itemSubkey = LWOOBJID_EMPTY;
			const auto actor = context;
			// Checking what is still missing and sending the mail happen together in one task per character, so two
			// restores can't both send the same thing
			const bool queued = Background::Run(TaskName(character.id), [characterId = character.id, xml = std::move(target->xml), wanted, mail](GameDatabase& db) -> nlohmann::json {
				const auto diff = Scan(db, characterId, xml);
				std::vector<InventoryRestore::Mail> plan;
				for (const auto& missing : diff.missing) {
					const auto it = wanted.find(missing.lot);
					if (it == wanted.end()) continue;
					for (const auto& one : InventoryRestore::Plan(missing, it->second)) plan.push_back(one);
				}
				if (plan.size() > MAX_MAILS) return { {"error", "That would take " + std::to_string(plan.size()) + " mails; give back at most " + std::to_string(MAX_MAILS) + " at once"} };
				nlohmann::json sent = nlohmann::json::array();
				for (const auto& one : plan) {
					auto copy = mail;
					copy.timeSent = static_cast<uint64_t>(std::time(nullptr));
					copy.itemID = one.originalId;
					copy.itemLOT = one.lot;
					copy.itemCount = static_cast<int16_t>(one.count);
					db.InsertNewMail(copy);
					sent.push_back({ one.lot, one.count, one.originalId != LWOOBJID_EMPTY });
				}
				return { {"sent", sent} };
			}, [=](nlohmann::json result, const std::string& error) {
				if (!error.empty()) return PlayerActions::Finish(requestId, { false, "Giving the items back failed: " + error });
				if (result.contains("error")) return PlayerActions::Finish(requestId, { false, result["error"].get<std::string>() });
				const auto& sent = result["sent"];
				if (sent.empty()) return PlayerActions::Finish(requestId, { false, "Nothing was sent: " + character.name + " isn't missing any of those any more (they hold them, have them in their mailbox, or someone else does)." });
				std::map<LOT, uint64_t> totals;
				size_t original = 0;
				for (const auto& one : sent) { totals[one[0].get<LOT>()] += one[1].get<uint64_t>(); original += one[2].get<bool>(); }
				std::string what;
				for (const auto& [lot, count] : totals) what += (what.empty() ? "" : ", ") + std::to_string(count) + "x " + ClientAssets::ItemName(lot) + " (" + std::to_string(lot) + ")";
				char when[32];
				const std::time_t time = takenAt;
				std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M UTC", std::gmtime(&time));
				Audit(actor, "restore_items", "Mailed " + character.name + " " + what + " lost since the snapshot from " + when + " (" + std::to_string(sent.size()) + " mail(s), " +
					std::to_string(original) + " with their original IDs)", AuditTarget::Character(character.id));
				BroadcastTableChanged("mail", std::to_string(character.id));
				PlayerActions::Finish(requestId, { true, "Mailed " + what + " to " + character.name + " in " + std::to_string(sent.size()) + " mail(s)." });
			});
			if (!queued) {
				PlayerActions::Finish(requestId, { false, "Items are already being given back to " + character.name + "; wait for that to finish" });
			}
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
