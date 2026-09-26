#include "MaintenanceRoutes.h"

#include <chrono>
#include <map>
#include <set>
#include <sstream>

#include "RouteUtils.h"
#include "ServerState.h"
#include "WSRoutes.h"
#include "Background.h"
#include "Scheduler.h"
#include "PlayerActions.h"
#include "ClientAssets.h"
#include "Database.h"
#include "CDClientDatabase.h"
#include "Game.h"
#include "Logger.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "Lxfml.h"
#include "Sd0.h"
#include "tinyxml2.h"
#include "CharacterXml.h"
#include "ObjectIDManager.h"

using namespace RouteUtils;

namespace {
	// Requests are capped at 3 MB by the web server, and JSON escaping makes the body bigger than the LXFML
	constexpr size_t MAX_IMPORT_BYTES = 2 * 1024 * 1024;

	bool AnyoneOnline() {
		const auto state = ServerState::GetServerStateJson();
		return state["stats"].value("onlinePlayers", 0u) > 0;
	}

	// Every pet in every inventory: pet id -> the character holding it
	std::map<LWOOBJID, LWOOBJID> PetHolders(GameDatabase& db, size_t& characters) {
		std::map<LWOOBJID, LWOOBJID> holders;
		db.ForEachCharacterXml([&](LWOOBJID characterId, const std::string& xml) {
			characters++;
			for (const auto& pet : CharacterXml::Pets(xml)) holders[pet.id] = characterId;
		});
		return holders;
	}

	// Pets named before the game saved owners: look each one up in the inventories once (0: nobody has it)
	void FillInPetOwners(Scheduler::RunPtr run) {
		const auto unknown = Database::Get()->GetPetsWithUnknownOwner();
		if (unknown.empty()) return run->Finish(true, "Every pet name has its owner");
		run->Log(std::to_string(unknown.size()) + " pet name(s) without an owner; reading inventories");
		const bool queued = Background::Run("pet_owners", [unknown](GameDatabase& db) -> nlohmann::json {
			size_t characters = 0;
			const auto holders = PetHolders(db, characters);
			size_t found = 0;
			for (const auto pet : unknown) {
				const auto holder = holders.find(pet);
				db.SetPetOwner(pet, holder == holders.end() ? 0 : holder->second);
				if (holder != holders.end()) found++;
			}
			return { {"characters", characters}, {"found", found}, {"missing", unknown.size() - found} };
		}, [run](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			BroadcastTableChanged("pet_names");
			run->Finish(true, "Read " + std::to_string(result["characters"].get<size_t>()) + " characters: " + std::to_string(result["found"].get<size_t>()) +
				" owner(s) found, " + std::to_string(result["missing"].get<size_t>()) + " pet(s) no one has any more");
		});
		if (!queued) run->Finish(false, "Already running");
	}

	bool PropertyInstanceRunning(LWOCLONEID cloneId) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		return std::ranges::any_of(ServerState::g_WorldInstances, [cloneId](const WorldInstanceInfo& world) { return world.cloneID == cloneId; });
	}
}

void RegisterMaintenanceTasks() {
	Scheduler::Register({ "lift_expired_bans", "Lift expired bans",
		"Unbans accounts whose temporary ban has ended (players are also let in at login once it has).", "5 * * * *",
		[](Scheduler::RunPtr run) {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			const auto lifted = Database::Get()->LiftExpiredBans(now);
			for (const auto accountId : lifted) {
				Database::Get()->InsertAccountNote({ 0, accountId, "unban", "Temporary ban ended", "[server]", now });
				BroadcastTableChanged("accounts", std::to_string(accountId));
			}
			run->Finish(true, std::to_string(lifted.size()) + " ban(s) lifted");
		} });
	Scheduler::Register({ "pet_owners", "Fill in pet owners",
		"The game saves who owns a pet when it is named. Pets named before that are looked up in the characters' inventories "
		"once; this does nothing when every pet name already has its owner.", "*/30 * * * *", FillInPetOwners });
	Scheduler::Register({ "pet_names_auto_approve", "Approve known pet names",
		"Approves pending pet names that were already approved for another pet.", "0 * * * *",
		[](Scheduler::RunPtr run) {
			const auto approved = Database::Get()->ApprovePreviouslyApprovedPetNames();
			if (approved > 0) {
				Database::Get()->InsertAuditLog(0, "[system]", "auto_approve_pet_names", std::to_string(approved) + " pet name(s)", 0, 0);
				BroadcastTableChanged("pet_names");
			}
			run->Finish(true, "Approved " + std::to_string(approved) + " pet name(s)");
		} });
}

void RegisterMaintenanceRoutes() {
	Route(eHTTPMethod::POST, "/api/maintenance/pet_names/auto_approve", Perm("maintenance"), "Approve pending pet names that were approved for another pet before",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto approved = Database::Get()->ApprovePreviouslyApprovedPetNames();
			Audit(context, "auto_approve_pet_names", std::to_string(approved) + " pet name(s)");
			if (approved) BroadcastTableChanged("pet_names");
			JsonSuccess(reply, { {"message", "Approved " + std::to_string(approved) + " pet name(s)"} });
		});

	Route(eHTTPMethod::POST, "/api/maintenance/pet_names/orphans", Perm("maintenance"),
		"Find (or with {delete: true}, remove) pet names whose pet no longer exists. Runs in the background: returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			const bool remove = body && body->value("delete", false);
			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(30));
			const auto actor = context;
			const bool queued = Background::Run("pet_name_orphans", [remove](GameDatabase& db) -> nlohmann::json {
				size_t characters = 0;
				const auto holders = PetHolders(db, characters);

				const auto names = db.GetAllPetNames();
				nlohmann::json orphans = nlohmann::json::array();
				for (const auto& [id, name] : names) {
					if (!holders.contains(id)) orphans.push_back({ {"id", std::to_string(id)}, {"name", name} });
				}
				nlohmann::json result{
					{"characters", characters}, {"petsFound", holders.size()}, {"petNames", names.size()},
					{"orphaned", orphans.size()}, {"sample", nlohmann::json(orphans.begin(), orphans.begin() + std::min<size_t>(orphans.size(), 20))}
				};
				if (!remove) return result;
				// A mismatch in how ids are stored would make every name look orphaned; never delete in that case
				if (!names.empty() && orphans.size() * 2 > names.size()) {
					result["refused"] = "Refusing to delete: more than half of all pet names look orphaned, which suggests an ID mismatch rather than real orphans";
					return result;
				}
				for (const auto& orphan : orphans) db.RejectPetName(std::stoll(orphan["id"].get<std::string>()));
				result["deleted"] = orphans.size();
				return result;
			}, [requestId, actor](nlohmann::json result, const std::string& error) {
				if (!error.empty()) return PlayerActions::Finish(requestId, { false, "The scan failed: " + error });
				if (result.contains("refused")) return PlayerActions::Finish(requestId, { false, result["refused"].get<std::string>(), result });
				if (result.contains("deleted")) {
					Audit(actor, "delete_orphaned_pet_names", std::to_string(result["deleted"].get<size_t>()) + " pet name(s)");
					BroadcastTableChanged("pet_names");
				}
				PlayerActions::Finish(requestId, { true, std::to_string(result["orphaned"].get<size_t>()) + " orphaned pet name(s)" +
					(result.contains("deleted") ? ", deleted" : ""), result });
			});
			if (!queued) {
				PlayerActions::Finish(requestId, { false, "This scan is already running" });
				return JsonError(reply, eHTTPStatusCode::CONFLICT, "This scan is already running");
			}
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::POST, "/api/maintenance/remove_buffs", Perm("maintenance"),
		"Remove every buff from every character (only while nobody is online). Body: {dry_run}. Runs in the background: returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			const bool dryRun = !body || body->value("dry_run", true);
			if (!dryRun && AnyoneOnline()) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Players are online; their worlds would overwrite this. Try again when nobody is online.");

			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(30));
			const auto actor = context;
			const bool queued = Background::Run("remove_buffs", [dryRun](GameDatabase& db) -> nlohmann::json {
				// First pass only records ids so no writes happen while the cursor is open
				std::vector<LWOOBJID> withBuffs;
				db.ForEachCharacterXml([&](LWOOBJID id, const std::string& xml) {
					if (xml.find("<buff") != std::string::npos) withBuffs.push_back(id);
				});
				if (dryRun) return { {"characters", withBuffs.size()}, {"dryRun", true} };

				uint32_t changed = 0;
				for (const auto id : withBuffs) {
					tinyxml2::XMLDocument doc;
					const auto xml = db.GetCharacterXml(id);
					if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) continue;
					auto* dest = doc.FirstChildElement("obj") ? doc.FirstChildElement("obj")->FirstChildElement("dest") : nullptr;
					auto* buff = dest ? dest->FirstChildElement("buff") : nullptr;
					if (!buff) continue;
					dest->DeleteChild(buff);
					tinyxml2::XMLPrinter printer(nullptr, true);
					doc.Print(&printer);
					db.UpdateCharacterXml(id, printer.CStr());
					changed++;
				}
				return { {"characters", changed}, {"dryRun", false} };
			}, [requestId, actor](nlohmann::json result, const std::string& error) {
				if (!error.empty()) return PlayerActions::Finish(requestId, { false, "Failed: " + error });
				const auto count = result["characters"].get<size_t>();
				if (result["dryRun"].get<bool>()) return PlayerActions::Finish(requestId, { true, std::to_string(count) + " character(s) have buffs", result });
				Audit(actor, "remove_all_buffs", std::to_string(count) + " character(s)");
				PlayerActions::Finish(requestId, { true, "Removed buffs from " + std::to_string(count) + " character(s)", result });
			});
			if (!queued) {
				PlayerActions::Finish(requestId, { false, "This is already running" });
				return JsonError(reply, eHTTPStatusCode::CONFLICT, "This is already running");
			}
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::POST, "/api/maintenance/fix_clone_ids", Perm("maintenance"), "Repair properties whose clone ID doesn't match their owner's",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto fixed = Database::Get()->FixPropertyCloneIds();
			Audit(context, "fix_property_clone_ids", std::to_string(fixed) + " propert(ies)");
			if (fixed) BroadcastTableChanged("properties");
			JsonSuccess(reply, { {"fixed", fixed}, {"message", "Fixed " + std::to_string(fixed) + " propert" + (fixed == 1 ? "y" : "ies")} });
		});

	Route(eHTTPMethod::GET, "/api/maintenance/missing_commendation_items", Perm("maintenance"), "Mission reward items with no commendation vendor price (game data check)",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto rows = CDClientDatabase::ExecuteQuery(
				"SELECT DISTINCT r.lot FROM ("
				"SELECT reward_item1 AS lot FROM Missions UNION SELECT reward_item2 FROM Missions "
				"UNION SELECT reward_item3 FROM Missions UNION SELECT reward_item4 FROM Missions) r "
				"LEFT JOIN ComponentsRegistry cr ON cr.id = r.lot AND cr.component_type = 11 "
				"LEFT JOIN ItemComponent ic ON ic.id = cr.component_id "
				"WHERE r.lot > 0 AND (ic.commendationLOT IS NULL OR ic.commendationCost IS NULL) ORDER BY r.lot;");
			nlohmann::json items = nlohmann::json::array();
			while (!rows.eof()) {
				const LOT lot = rows.getIntField("lot");
				items.push_back({ {"lot", lot}, {"name", ClientAssets::ItemName(lot)} });
				rows.nextRow();
			}
			JsonReply(reply, eHTTPStatusCode::OK, { {"items", items} });
		});

	Route(eHTTPMethod::POST, "/api/properties/:id/import", Perm("properties_import"), "Import LXFML models onto a property. Body: {lxfml, position: [x,y,z] (optional)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto propertyId = PathId<LWOOBJID>(context.path, 2);
			const auto body = ParseBody(context);
			if (!propertyId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto property = Database::Get()->GetPropertyInfo(*propertyId);
			if (!property) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
			const auto owner = Database::Get()->GetCharacterInfo(property->ownerId);
			if (!owner) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property owner not found");
			// A loaded property keeps its own list of models and saves it back, which would drop ours
			if (PropertyInstanceRunning(property->cloneId)) return JsonError(reply, eHTTPStatusCode::CONFLICT, "This property is loaded in a world right now; try again when nobody is on it");

			const std::string lxfml = body->value("lxfml", "");
			if (lxfml.empty() || lxfml.size() > MAX_IMPORT_BYTES) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Provide LXFML up to 2 MB");
			tinyxml2::XMLDocument check;
			if (check.Parse(lxfml.c_str()) != tinyxml2::XML_SUCCESS || !check.FirstChildElement("LXFML")) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That is not an LXFML file");

			std::optional<NiPoint3> position;
			if (body->contains("position") && (*body)["position"].is_array() && (*body)["position"].size() == 3) {
				const auto& p = (*body)["position"];
				position = NiPoint3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
			}

			// Split into separate models and centre each on its own origin, exactly as the game does for new builds
			const auto models = Lxfml::Split(lxfml);
			if (models.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No bricks found in that LXFML");

			nlohmann::json created = nlohmann::json::array();
			for (const auto& split : models) {
				// The world servers' ObjectIDManager, as for a model built in game
				const auto [modelId, blueprintId] = ObjectIDManager::GetNewModelIDs();
				// Start from an empty (header-only) sd0 buffer, then compress the model into it
				std::stringstream header(std::string(Sd0::SD0_HEADER, 5));
				Sd0 sd0(header);
				sd0.FromData(reinterpret_cast<const uint8_t*>(split.lxfml.data()), split.lxfml.size());
				auto stream = sd0.GetAsStream();
				Database::Get()->InsertNewUgcModel(stream, blueprintId, owner->accountId, owner->id);

				IPropertyContents::Model model;
				model.id = modelId;
				model.ugcId = blueprintId;
				model.lot = 14;
				model.position = position ? *position + split.center - models.front().center : split.center;
				model.rotation = QuatUtils::IDENTITY;
				Database::Get()->InsertNewPropertyModel(*propertyId, model, "Objects_14_name");
				created.push_back(std::to_string(modelId));
			}
			Audit(context, "import_property_models", std::to_string(created.size()) + " model(s) onto property " + std::to_string(*propertyId));
			BroadcastTableChanged("properties", std::to_string(*propertyId));
			JsonSuccess(reply, { {"models", created}, {"message", "Imported " + std::to_string(created.size()) + " model(s)"} });
		});
}
