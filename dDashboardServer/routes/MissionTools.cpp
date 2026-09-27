#include "MissionTools.h"

#include <algorithm>
#include <ctime>

#include "RouteUtils.h"
#include "CharacterTools.h"
#include "PlayerActions.h"
#include "master/PlayerAction.h"
#include "ClientAssets.h"
#include "GameLabels.h"
#include "WSRoutes.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Locale.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace MissionCatalog {
	const std::map<uint32_t, Info>& All() {
		static const auto missions = [] {
			std::map<uint32_t, Info> all;
			auto rows = CDClientDatabase::ExecuteQuery(
				"SELECT id, defined_type, defined_subtype, isMission, repeatable, locStatus, reward_currency, LegoScore, reward_reputation, "
				"reward_item1, reward_item1_count, reward_item2, reward_item2_count, reward_item3, reward_item3_count, reward_item4, reward_item4_count FROM Missions;");
			while (!rows.eof()) {
				Info info;
				info.id = static_cast<uint32_t>(rows.getIntField("id"));
				info.type = rows.getStringField("defined_type", "");
				info.subtype = rows.getStringField("defined_subtype", "");
				info.definition.isMission = rows.getIntField("isMission", 0) != 0;
				info.definition.repeatable = rows.getIntField("repeatable", 0) != 0;
				info.released = rows.getIntField("locStatus", 0) == 2;
				info.coins = rows.getInt64Field("reward_currency", 0);
				info.uscore = rows.getIntField("LegoScore", 0);
				info.reputation = rows.getInt64Field("reward_reputation", 0);
				for (int i = 1; i <= 4; i++) {
					const auto item = "reward_item" + std::to_string(i);
					const LOT lot = rows.getIntField(item.c_str(), -1);
					if (lot > 0) info.rewardItems.emplace_back(lot, std::max(rows.getIntField((item + "_count").c_str(), 1), 1));
				}
				all[info.id] = std::move(info);
				rows.nextRow();
			}
			// Tasks in table order, which is the order the game numbers them in (CDMissionTasksTable)
			auto tasks = CDClientDatabase::ExecuteQuery("SELECT id, uid, taskType, target, targetValue FROM MissionTasks;");
			while (!tasks.eof()) {
				const auto it = all.find(static_cast<uint32_t>(tasks.getIntField("id")));
				if (it != all.end()) {
					const auto type = static_cast<eMissionTaskType>(tasks.getIntField("taskType", -1));
					it->second.tasks.push_back({ static_cast<uint32_t>(tasks.getIntField("uid")), type, tasks.getIntField("target", 0),
						static_cast<uint32_t>(std::max(tasks.getIntField("targetValue", 0), 0)) });
					it->second.definition.tasks.push_back(type);
				}
				tasks.nextRow();
			}
			return all;
		}();
		return missions;
	}

	const Info* Find(uint32_t id) {
		const auto it = All().find(id);
		return it == All().end() ? nullptr : &it->second;
	}

	const MissionXml::Definition* Definition(uint32_t id) {
		const auto* info = Find(id);
		return info ? &info->definition : nullptr;
	}

	std::string Name(uint32_t id) {
		const auto& name = Locale::GetPhrase("Missions_" + std::to_string(id) + "_name");
		return name.empty() ? "Mission " + std::to_string(id) : name;
	}

	std::string TaskText(uint32_t uid) {
		return Locale::GetPhrase("MissionTasks_" + std::to_string(uid) + "_description");
	}
}

namespace {
	std::string Kind(const MissionCatalog::Info* info) {
		return info && !info->definition.isMission ? "Achievement" : "Mission";
	}

	nlohmann::json Rewards(const MissionCatalog::Info& info) {
		nlohmann::json items = nlohmann::json::array();
		for (const auto& [lot, count] : info.rewardItems) items.push_back({ {"lot", lot}, {"name", ClientAssets::ItemName(lot)}, {"count", count} });
		return { {"coins", info.coins}, {"uscore", info.uscore}, {"reputation", info.reputation}, {"items", items} };
	}

	nlohmann::json MissionJson(const MissionXml::Entry& entry) {
		const auto* info = MissionCatalog::Find(entry.id);
		nlohmann::json tasks = nlohmann::json::array();
		if (info) {
			for (size_t i = 0; i < info->tasks.size(); i++) {
				const auto& task = info->tasks[i];
				// A finished mission keeps no task progress; all of it was done
				const auto progress = entry.current && i < entry.tasks.size() ? entry.tasks[i].progress : (entry.Done() ? task.targetValue : 0u);
				tasks.push_back({ {"description", MissionCatalog::TaskText(task.uid)}, {"type", GameLabels::Name(task.type)},
					{"progress", std::min(progress, task.targetValue)}, {"target", task.targetValue} });
			}
		}
		return {
			{"id", entry.id}, {"name", MissionCatalog::Name(entry.id)}, {"kind", Kind(info)},
			{"type", info ? info->type : ""}, {"subtype", info ? info->subtype : ""}, {"known", info != nullptr},
			{"repeatable", info && info->definition.repeatable}, {"state", static_cast<int>(entry.state)}, {"state_name", GameLabels::Name(entry.state)},
			{"done", entry.Done()}, {"current", entry.current}, {"completions", entry.completions}, {"completed_at", entry.completedAt},
			{"tasks", tasks}, {"rewards", info ? Rewards(*info) : nullptr}
		};
	}

	ePlayerAction ActionFor(MissionXml::eChange change) {
		switch (change) {
		case MissionXml::eChange::COMPLETE: return ePlayerAction::MISSION_COMPLETE;
		case MissionXml::eChange::RESET: return ePlayerAction::MISSION_RESET;
		case MissionXml::eChange::ACCEPT: break;
		}
		return ePlayerAction::MISSION_ACCEPT;
	}
}

void RegisterMissionToolRoutes() {
	Route(eHTTPMethod::GET, "/api/characters/:id/missions", 0,
		"A character's missions and achievements from its saved data, with names, tasks, state and rewards from the game data. "
		"Also {changes} a mission can get with characters_missions (rewards: whether it can give the mission's rewards)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
			nlohmann::json missions = nlohmann::json::array();
			for (const auto& [id, entry] : MissionXml::Read(Database::Get()->GetCharacterXml(*charId), MissionCatalog::Definition)) missions.push_back(MissionJson(entry));
			nlohmann::json changes = nlohmann::json::array();
			for (const auto change : magic_enum::enum_values<MissionXml::eChange>()) {
				changes.push_back({ {"value", std::string(magic_enum::enum_name(change))}, {"name", GameLabels::Name(change)}, {"rewards", change == MissionXml::eChange::COMPLETE} });
			}
			JsonSuccess(reply, { {"missions", missions}, {"changes", changes} });
		});

	Route(eHTTPMethod::GET, "/api/missions", Perm("characters_missions"), "Find missions and achievements by name or ID. Query: ?q=",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto query = QueryValue(context.queryString, "q");
			std::ranges::transform(query, query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto id = GeneralUtils::TryParse<uint32_t>(query);
			nlohmann::json found = nlohmann::json::array();
			if (query.size() < 2 && !id) return JsonSuccess(reply, { {"missions", found} });
			for (const auto& [missionId, info] : MissionCatalog::All()) {
				auto name = MissionCatalog::Name(missionId);
				auto lower = name;
				std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (id ? missionId != *id : lower.find(query) == std::string::npos) continue;
				found.push_back({ {"id", missionId}, {"name", name}, {"kind", Kind(&info)}, {"type", info.type}, {"subtype", info.subtype} });
				if (found.size() >= 50) break;
			}
			JsonSuccess(reply, { {"missions", found} });
		});

	Route(eHTTPMethod::POST, "/api/characters/:id/missions/:mission", Perm("characters_missions"),
		"Change a character's mission. Body: {change: COMPLETE | RESET | ACCEPT, rewards (COMPLETE: give its rewards; only while the player is in game)}. "
		"In game it is done by their world as the GM commands do; otherwise their saved data is changed (the owner is disconnected from "
		"character select first and a snapshot is kept). Returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto missionId = PathId<uint32_t>(context.path, 4);
			const auto body = ParseBody(context);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			const auto* mission = missionId ? MissionCatalog::Find(*missionId) : nullptr;
			if (!mission) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No such mission in the game data");
			if (!body || !(*body)["change"].is_string()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick what to do with the mission");
			const auto change = magic_enum::enum_cast<MissionXml::eChange>((*body)["change"].get<std::string>(), magic_enum::case_insensitive);
			if (!change) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown change");
			if (!AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::ITEMS)) return;
			const bool rewards = *change == MissionXml::eChange::COMPLETE && body->value("rewards", false);

			const auto requestId = PlayerActions::Begin(context.accountId);
			const auto actor = context;
			const auto character = *info;
			const auto id = *missionId;
			const auto name = MissionCatalog::Name(id);
			const auto definition = mission->definition;
			const auto what = GameLabels::Name(*change) + " " + Kind(mission) + " " + std::to_string(id) + " \"" + name + "\"";
			PlayerActionRequest request;
			request.action = ActionFor(*change);
			request.characterId = character.id;
			request.accountId = character.accountId;
			request.targetId = id;
			request.approved = rewards;
			request.text = name;
			PlayerActions::Request(request, actor.accountId, [=](const PlayerActionResult& result) {
				// In game: done by the world, which answers 2 when there was nothing to do
				if (result.affected == 2) {
					PlayerActions::Finish(requestId, { false, character.name + " is in game and there is nothing to " + GameLabels::Name(*change) + " for " + name + "." });
				} else if (result.affected > 0) {
					Audit(actor, "mission_change", character.name + " (in game): " + what + (*change == MissionXml::eChange::COMPLETE ? (rewards ? " with rewards" : " without rewards") : ""),
						AuditTarget::Character(character.id));
					BroadcastTableChanged("characters", std::to_string(character.id));
					PlayerActions::Finish(requestId, { true, "Done in game for " + character.name + ".", nullptr, 1 });
				} else if (result.timedOut) {
					PlayerActions::Finish(requestId, { false, "Not every world server answered, so " + character.name + " may be in game. Nothing was changed; try again." });
				} else if (rewards) {
					PlayerActions::Finish(requestId, { false, character.name + " isn't in game, and rewards can only be given in game. Complete it without rewards, or again while they're online." });
				} else {
					// Not in game: change the saved data as the game would save it
					const auto change_ = *change;
					WriteCharacterXml(character.id, character.accountId, actor.authenticatedUser, actor.accountId, "mission change",
						[=](const std::string& current, std::string& error) {
							return MissionXml::Change(current, id, definition, change_, static_cast<uint32_t>(std::time(nullptr)), error);
						},
						[what, change_](const std::string&, const std::string&) { return what + (change_ == MissionXml::eChange::COMPLETE ? " without rewards" : ""); },
						[requestId](PlayerActions::Outcome outcome) { if (outcome.success) outcome.affected = 1; PlayerActions::Finish(requestId, outcome); });
				}
				return PlayerActions::Outcome{ true, "" };
			});
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
