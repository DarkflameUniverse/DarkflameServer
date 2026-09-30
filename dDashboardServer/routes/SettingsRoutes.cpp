#include "SettingsRoutes.h"
#include "GameText.h"
#include "MasterPackets.h"
#include "Permissions.h"
#include "SettingsCatalog.h"
#include "SettingsHistory.h"
#include "SlashCommandLevels.h"
#include "AccountRules.h"

#include <array>
#include <map>
#include <regex>

#include "RouteUtils.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "CDClientDatabase.h"
#include "BitStreamUtils.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "WSRoutes.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "dServer.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	// Where a setting's value comes from, in the order dConfig applies them, and that value (nullopt: the default)
	std::pair<std::string, std::optional<std::string>> Effective(const IServerConfig::Setting* row) {
		if (!row) return { "default", std::nullopt };
		if (row->webWins && row->webValue) return { "web", row->webValue };
		if (!row->fileSource.empty() && row->fileValue && !row->fileValue->empty()) return { row->fileSource, row->fileValue };
		if (!row->fileSource.empty() && dConfig::IsSecretKey(row->name)) return { row->fileSource, std::nullopt }; // set in the file, value not copied
		if (row->webValue && !row->webValue->empty()) return { "web", row->webValue };
		return { "default", std::nullopt };
	}

	nlohmann::json ConditionJson(const std::optional<SettingsCatalog::Condition>& condition) {
		if (!condition) return nullptr;
		return { {"file", condition->file}, {"key", condition->key}, {"values", condition->values} };
	}

	std::string EnvironmentName(const std::string& name) {
		std::string upper = name;
		std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
		return upper;
	}

	/**
	 * One setting for the page: its description from the catalog (if known), and what the servers reported.
	 * Secrets never leave the server; the page only learns whether one is set.
	 */
	nlohmann::json SettingJson(const std::string& file, const std::string& name, const SettingsCatalog::Setting* info, const IServerConfig::Setting* row) {
		const bool secret = dConfig::IsSecretKey(name) || (info && info->type == SettingsCatalog::eType::SECRET);
		const auto [source, value] = Effective(row);
		const auto* section = info ? SettingsCatalog::FindSection(info->section) : nullptr;
		nlohmann::json json{
			{"file", file}, {"name", name}, {"env", EnvironmentName(name)}, {"known", info != nullptr},
			{"title", info ? info->title : name}, {"section", info ? info->section : "Custom settings"},
			{"category", section ? section->category : "other"},
			// Known settings use the catalog's description (the .ini comments are for people editing the file)
			// Game names in descriptions are locale keys (%[ZoneTable_1000_DisplayDescription]), shown in the viewer's language
			{"description", GameText::Expand(info ? info->description : row ? row->description : "")},
			{"type", info ? SettingsCatalog::TypeName(info->type) : "text"}, {"default", info ? info->defaultValue : ""},
			{"unit", info ? info->unit : ""}, {"format", info ? SettingsCatalog::FormatName(info->format) : "plain"},
			{"listOf", info ? SettingsCatalog::ListOfName(info->listOf) : "number"}, {"condition", ConditionJson(info ? info->condition : std::nullopt)},
			{"restart", info && info->restart}, {"unused", info && info->unused}, {"multiline", info && info->multiline},
			{"fileOnly", dConfig::IsFileOnlyKey(name)}, {"secret", secret},
			{"source", source}, {"reported", row && row->seenAt > 0},
			{"fileSource", row ? row->fileSource : ""}, {"webWins", row && row->webWins},
			{"hasWebValue", row && row->webValue.has_value()},
			{"updatedAt", row ? row->updatedAt : 0}, {"updatedBy", row ? row->updatedBy : ""}
		};
		if (info) {
			if (info->min) json["min"] = *info->min;
			if (info->max) json["max"] = *info->max;
			if (!info->choices.empty()) json["choices"] = info->choices;
			if (!info->choiceLabels.empty()) json["choiceLabels"] = info->choiceLabels;
		}
		// The database connection and the dashboard's keys never come from the database: show what this server read
		if (dConfig::IsFileOnlyKey(name)) {
			const auto env = dConfig::GetEnvironmentValue(name);
			const auto local = Game::config->GetValue(name);
			std::string where = env ? "env" : "default";
			if (!env) for (const auto& entry : Game::config->GetFileEntries()) if (entry.key == name && !entry.value.empty()) where = "file";
			json["source"] = where;
			json["fileSource"] = where == "default" ? "" : where;
			if (secret) json["isSet"] = !local.empty();
			else json["value"] = local.empty() && info ? info->defaultValue : local;
			return json;
		}
		if (secret) {
			json["isSet"] = source != "default";
		} else {
			json["value"] = value ? *value : info ? info->defaultValue : "";
			json["fileValue"] = row && row->fileValue ? nlohmann::json(*row->fileValue) : nlohmann::json(nullptr);
			json["webValue"] = row && row->webValue ? nlohmann::json(*row->webValue) : nlohmann::json(nullptr);
		}
		return json;
	}

	// Names for the numbers in a list setting: zones, items or reward codes
	nlohmann::json ListNames(const std::string& of, const std::vector<int64_t>& ids) {
		nlohmann::json names = nlohmann::json::object();
		const auto& zones = GameText::ZoneNames();
		for (const auto id : ids) {
			const auto key = std::to_string(id);
			if (of == "zone") {
				if (zones.contains(key)) names[key] = zones[key];
			} else if (of == "lot") {
				const auto name = ClientAssets::ItemName(static_cast<LOT>(id));
				if (!name.empty()) names[key] = name;
			} else if (of == "reward_code") {
				auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT code FROM RewardCodes WHERE id = ? LIMIT 1;");
				stmt.bind(1, static_cast<int>(id));
				auto result = stmt.execQuery();
				if (!result.eof()) names[key] = result.getStringField(0);
			}
		}
		return names;
	}

	// Everything a list setting can hold, for the page's picker: zones or reward codes (items are too many to list)
	nlohmann::json ListOptions(const std::string& of) {
		nlohmann::json options = nlohmann::json::array();
		if (of == "zone") {
			std::vector<std::pair<int64_t, std::string>> zones;
			for (const auto& [id, name] : GameText::ZoneNames().items()) {
				if (const auto zoneId = GeneralUtils::TryParse<int64_t>(id); zoneId && *zoneId > 0) zones.emplace_back(*zoneId, name.get<std::string>());
			}
			std::sort(zones.begin(), zones.end());
			for (const auto& [id, name] : zones) options.push_back({ {"id", id}, {"label", name}, {"detail", ""} });
		} else if (of == "reward_code") {
			// What the client does with the two codes that don't just give an item
			const std::map<int64_t, std::string> effects{ { 4, "Opens " + GameText::ZoneName(1700) }, { 30, "Bricks aren't used up in build mode" } };
			auto result = CDClientDatabase::ExecuteQuery("SELECT id, code, attachmentLOT FROM RewardCodes ORDER BY id;");
			for (; !result.eof(); result.nextRow()) {
				const int64_t id = result.getIntField(0);
				const LOT lot = result.fieldIsNull(2) ? 0 : result.getIntField(2);
				std::string detail = effects.contains(id) ? effects.at(id) : "";
				if (lot > 0) detail += (detail.empty() ? "" : "; ") + std::string("mails ") + ClientAssets::ItemName(lot);
				options.push_back({ {"id", id}, {"label", result.getStringField(1)}, {"detail", detail} });
			}
		}
		return options;
	}

	std::vector<int64_t> ParseIds(const std::string& text) {
		std::vector<int64_t> ids;
		for (const auto& part : GeneralUtils::SplitString(text, ',')) {
			if (const auto id = GeneralUtils::TryParse<int64_t>(part)) ids.push_back(*id);
			if (ids.size() >= 200) break;
		}
		return ids;
	}

	// Tell every server to reload its settings: this dashboard now, the rest through master
	void ReloadEverywhere() {
		Game::config->ReloadConfig();
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return;
		MasterPackets::SendToMaster(MasterPackets::ConfigReload());
	}

	bool ValidName(const std::string& name) {
		static const std::regex pattern("^[a-z0-9_]{1,128}$");
		return std::regex_match(name, pattern);
	}

	// permission_* and command_level_* settings belong to the Permissions page, which needs its own permission
	bool IsPermissionSetting(const std::string& name) {
		return Permissions::IsPermissionSetting(name) || SlashCommandLevels::IsLevelSetting(name);
	}

	/**
	 * Where a slash command's level comes from on the world servers and its value (nullopt: the default), in the order
	 * they apply settings: a web value that wins, their environment or worldconfig.ini, sharedconfig.ini, then any
	 * other web value. The file and environment values are the ones the worlds last reported.
	 */
	std::pair<std::string, std::optional<std::string>> CommandLevelValue(const std::string& name, const std::vector<IServerConfig::Setting>& rows) {
		const auto find = [&](std::string_view file) -> const IServerConfig::Setting* {
			const auto it = std::ranges::find_if(rows, [&](const auto& row) { return row.file == file && row.name == name; });
			return it == rows.end() ? nullptr : &*it;
		};
		const std::array<const IServerConfig::Setting*, 2> order{ find(SlashCommandLevels::CONFIG_FILE), find("sharedconfig.ini") };
		for (const auto* row : order) if (row && row->webWins && row->webValue) return { "web", row->webValue };
		for (const auto* row : order) if (row && !row->fileSource.empty() && row->fileValue && !row->fileValue->empty()) return { row->fileSource, row->fileValue };
		for (const auto* row : order) if (row && row->webValue && !row->webValue->empty()) return { "web", row->webValue };
		return { "default", std::nullopt };
	}

	struct CommandLevel {
		uint8_t level;
		std::string source;       // like PermissionSource; "permission" when it follows its dashboard permission
		bool overridden{};        // a paired command with its own command_level_<name> value
		bool followSet{};         // command_level_<name>=permission set here, so a file's value is ignored
		std::string localSource;  // where a file or environment value comes from, if any ("file", "env")
		bool keptFromUpgrade{};   // the override is the level it had before it followed the permission, kept by the upgrade
	};

	/**
	 * A command's level as the world servers work it out (SlashCommandHandler::GetRequiredLevel). A command paired with
	 * a dashboard permission uses that permission's level unless it has its own value (an override, e.g. from before
	 * they were one setting).
	 */
	CommandLevel ResolveCommandLevel(const ISlashCommands::SlashCommand& command, const std::vector<IServerConfig::Setting>& rows) {
		if (command.fixed) return { command.defaultLevel, "fixed" };
		const auto name = SlashCommandLevels::ConfigName(command.name);
		const auto [source, value] = CommandLevelValue(name, rows);
		std::string localSource;
		bool byUpgrade = false;
		for (const auto& row : rows) {
			if (row.name == name && !row.fileSource.empty() && row.fileValue && !row.fileValue->empty()) localSource = row.fileSource;
			if (row.name == name && row.file == SlashCommandLevels::CONFIG_FILE && row.webValue && row.updatedBy == SlashCommandLevels::UPGRADE_ACTOR) byUpgrade = true;
		}
		if (const auto* permission = Permissions::Find(command.dashboardPermission)) {
			const auto paired = SlashCommandLevels::ResolvePaired(command.defaultLevel, command.minLevel, false, Permissions::Level(permission->key), value.value_or(""));
			if (paired.overridden) return { paired.level, source, true, false, localSource, source == "web" && byUpgrade };
			const bool follow = value && *value == SlashCommandLevels::FOLLOW_PERMISSION;
			const bool bad = value && !follow;
			return { paired.level, bad ? "permission (bad value in " + source + ")" : "permission", false, follow && source == "web", localSource };
		}
		if (!value) return { command.defaultLevel, source, false, false, localSource };
		const bool usable = SlashCommandLevels::ParseLevel(command.minLevel, *value).has_value();
		return { SlashCommandLevels::Resolve(command.defaultLevel, command.minLevel, false, *value), usable ? source : "default (bad value in " + source + ")", false, false, localSource };
	}

	// "/kick, /ban": the commands that use a permission's level
	std::string FollowingCommands(const std::string& key, const std::vector<ISlashCommands::SlashCommand>& commands, const std::vector<IServerConfig::Setting>& rows) {
		std::string list;
		for (const auto& command : commands) {
			if (command.dashboardPermission != key || command.fixed || command.aliases.empty()) continue;
			if (ResolveCommandLevel(command, rows).overridden) continue;
			list += (list.empty() ? "/" : ", /") + command.aliases.front();
		}
		return list;
	}

	std::vector<IServerConfig::Setting> CommandLevelRows() {
		auto rows = Database::Get()->GetServerConfig({ std::string(SlashCommandLevels::CONFIG_FILE), "sharedconfig.ini" });
		std::erase_if(rows, [](const auto& row) { return !SlashCommandLevels::IsLevelSetting(row.name); });
		return rows;
	}

	// Where a permission's level comes from, in the same order dConfig applies them; an unusable value means the default
	std::string PermissionSource(const Permissions::Permission& permission, const std::map<std::string, IServerConfig::Setting>& web) {
		if (permission.locked) return "fixed";
		const auto name = Permissions::ConfigName(permission.key);
		std::optional<std::pair<std::string, std::string>> source; // {where, value}
		const auto webRow = web.find(name);
		const bool hasWeb = webRow != web.end() && webRow->second.webValue.has_value();
		if (hasWeb && webRow->second.webWins) source = { "web", *webRow->second.webValue };
		else if (const auto env = dConfig::GetEnvironmentValue(name)) source = { "env", *env };
		else {
			for (const auto& entry : Game::config->GetFileEntries()) {
				if (entry.key == name && !entry.value.empty()) source = { "file", entry.value };
			}
			if (!source && hasWeb) source = { "web", *webRow->second.webValue };
		}
		if (!source) return "default";
		const auto parsed = GeneralUtils::TryParse<int32_t>(source->second);
		const bool usable = parsed && *parsed >= permission.minLevel && *parsed <= Permissions::MAX_LEVEL;
		return usable ? source->first : "default (bad value in " + source->first + ")";
	}

	bool ValidFile(const std::string& file) {
		static const std::regex pattern("^[a-z0-9_]{1,50}\\.ini$");
		return std::regex_match(file, pattern);
	}

	struct Change {
		std::string file;
		std::string name;
		std::optional<std::string> value; // nullopt: clear the value set on the web
		bool webWins{};
	};

	// Read and check one change from the page; the value is put in the form the servers read
	std::optional<Change> ParseChange(const nlohmann::json& body, std::string& error) {
		Change change{ body.value("file", ""), body.value("name", "") };
		if (!ValidFile(change.file)) { error = "Pick a config file such as sharedconfig.ini"; return std::nullopt; }
		if (!ValidName(change.name)) { error = "Setting names use lowercase letters, digits and _"; return std::nullopt; }
		if (dConfig::IsFileOnlyKey(change.name)) {
			error = change.name + " can only be set in " + change.file + " or the " + EnvironmentName(change.name) + " environment variable";
			return std::nullopt;
		}
		if (IsPermissionSetting(change.name)) { error = "Change permissions on the Permissions page"; return std::nullopt; }

		const auto& raw = body.contains("value") ? body["value"] : nlohmann::json(nullptr);
		if (raw.is_string()) change.value = raw.get<std::string>();
		else if (raw.is_boolean()) change.value = raw.get<bool>() ? "1" : "0";
		else if (raw.is_number()) change.value = raw.dump();
		else if (raw.is_array()) {
			std::string joined;
			for (const auto& item : raw) joined += (joined.empty() ? "" : ",") + (item.is_string() ? item.get<std::string>() : item.dump());
			change.value = joined;
		}
		// Known settings are checked and stored the way the servers read them (1/0, plain numbers, clean lists)
		if (change.value) {
			if (const auto* info = SettingsCatalog::Find(change.file, change.name)) {
				std::string why;
				const auto normalized = SettingsCatalog::Normalize(*info, *change.value, why);
				if (!normalized) { error = info->title + ": " + why; return std::nullopt; }
				change.value = normalized;
			} else if (change.value->size() > 4000 || change.value->find('\n') != std::string::npos) {
				error = "Values are one line of up to 4000 characters";
				return std::nullopt;
			}
		}
		change.webWins = change.value.has_value() && body.value("webWins", false);
		return change;
	}

	void ApplyChange(const HTTPContext& context, const Change& change, uint64_t revertOf = 0) {
		const auto before = SettingsHistory::Current(change.file, change.name);
		Database::Get()->SetWebConfigValue(change.file, change.name, change.value, change.webWins, context.authenticatedUser);
		SettingsHistory::Record(context, change.file, change.name, before, change.value, change.webWins, false, revertOf);
		const auto shown = !change.value ? std::string("cleared") : dConfig::IsSecretKey(change.name) ? std::string("<secret>") : "\"" + *change.value + "\"";
		Audit(context, "change_setting", change.file + " " + change.name + " = " + shown + (change.webWins ? " (web value wins)" : ""));
	}
}

std::vector<SlashCommandNow> CurrentSlashCommands() {
	std::vector<SlashCommandNow> commands;
	std::vector<ISlashCommands::SlashCommand> rows;
	try {
		rows = Database::Get()->GetSlashCommands();
	} catch (const std::exception&) {
		return commands; // no slash_commands table yet: no world has started
	}
	const auto levels = CommandLevelRows();
	for (auto& row : rows) {
		SlashCommandNow command;
		command.rules = { row.name, ResolveCommandLevel(row, levels).level, row.minLevel, row.fixed,
			Permissions::Find(row.dashboardPermission) ? row.dashboardPermission : "" };
		command.aliases = std::move(row.aliases);
		command.help = std::move(row.help);
		command.clientHandled = row.clientHandled;
		commands.push_back(std::move(command));
	}
	return commands;
}

std::optional<std::string> SaveSetting(const HTTPContext& context, const nlohmann::json& body, uint64_t revertOf) {
	std::string error;
	const auto change = ParseChange(body, error);
	if (!change) return error;
	ApplyChange(context, *change, revertOf);
	ReloadEverywhere();
	BroadcastTableChanged("settings");
	return std::nullopt;
}

void RegisterSettingsRoutes() {
	Route(eHTTPMethod::GET, "/api/settings", Perm("settings"),
		"Every server setting, grouped by config file: its type, default, valid values, current value and where that comes from (secrets hidden)",
		[](HTTPReply& reply, const HTTPContext&) {
			std::map<std::pair<std::string, std::string>, IServerConfig::Setting> rows;
			for (auto& row : Database::Get()->GetServerConfig({})) {
				if (IsPermissionSetting(row.name)) continue;
				rows[{ row.file, row.name }] = std::move(row);
			}
			nlohmann::json settings = nlohmann::json::array();
			// Everything the code reads, whether or not any .ini or server mentions it...
			for (const auto& info : SettingsCatalog::All()) {
				const auto it = rows.find({ info.file, info.key });
				settings.push_back(SettingJson(info.file, info.key, &info, it == rows.end() ? nullptr : &it->second));
				if (it != rows.end()) rows.erase(it);
			}
			// ...then anything else the servers reported or someone added on the web
			for (const auto& [id, row] : rows) settings.push_back(SettingJson(row.file, row.name, nullptr, &row));

			nlohmann::json files = nlohmann::json::array();
			for (const auto& [file, name] : SettingsCatalog::Files()) files.push_back({ {"file", file}, {"name", name} });
			nlohmann::json categories = nlohmann::json::array();
			for (const auto& category : SettingsCatalog::Categories()) categories.push_back({ {"id", category.id}, {"name", category.name}, {"description", GameText::Expand(category.description)} });
			categories.push_back({ {"id", "other"}, {"name", "Custom"}, {"description", "Settings this page doesn't know, e.g. from a modified server."} });
			nlohmann::json sections = nlohmann::json::array();
			for (const auto& section : SettingsCatalog::Sections()) {
				sections.push_back({ {"category", section.category}, {"name", section.name}, {"description", GameText::Expand(section.description)},
					{"layout", SettingsCatalog::LayoutName(section.layout)}, {"condition", ConditionJson(section.condition)} });
			}
			sections.push_back({ {"category", "other"}, {"name", "Custom settings"}, {"description", ""}, {"layout", "rows"}, {"condition", nullptr} });
			// Names for what list settings hold now (zones, items, reward codes)
			nlohmann::json names = nlohmann::json::object();
			for (const auto& setting : settings) {
				if (setting["type"] != "int_list" || setting["listOf"] == "number" || !setting.contains("value")) continue;
				names[setting["file"].get<std::string>() + "/" + setting["name"].get<std::string>()] = ListNames(setting["listOf"], ParseIds(setting["value"]));
			}
			JsonReply(reply, eHTTPStatusCode::OK, { {"success", true}, {"categories", categories}, {"sections", sections}, {"files", files},
				{"settings", settings}, {"names", names} });
		});

	Route(eHTTPMethod::POST, "/api/settings", Perm("settings"),
		"Set a setting on the web. Body: {file, name, value (null to clear the web value), webWins (beat the file and environment)}. Servers reload at once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			std::string error;
			const auto change = ParseChange(*body, error);
			if (!change) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
			ApplyChange(context, *change);
			ReloadEverywhere();
			BroadcastTableChanged("settings");
			const auto* info = SettingsCatalog::Find(change->file, change->name);
			JsonSuccess(reply, { {"value", change->value ? nlohmann::json(dConfig::IsSecretKey(change->name) ? "" : *change->value) : nlohmann::json(nullptr)},
				{"message", info && info->restart ? "Saved. Restart the servers for this one to take effect." : "Saved. Servers reloaded their settings."} });
		});

	Route(eHTTPMethod::POST, "/api/settings/batch", Perm("settings"),
		"Save several settings at once. Body: {changes: [{file, name, value, webWins}]}. Nothing is saved unless every change is valid; "
		"a failure replies {errors: {\"file/name\": message}}. Servers reload once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->contains("changes") || !(*body)["changes"].is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send {changes: [...]}");
			const auto& list = (*body)["changes"];
			if (list.empty() || list.size() > 200) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send 1 to 200 changes");
			std::vector<Change> changes;
			nlohmann::json errors = nlohmann::json::object();
			for (const auto& item : list) {
				std::string error;
				if (!item.is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Each change is an object");
				if (auto change = ParseChange(item, error)) changes.push_back(std::move(*change));
				else errors[item.value("file", "") + "/" + item.value("name", "")] = error;
			}
			if (!errors.empty()) {
				JsonReply(reply, eHTTPStatusCode::BAD_REQUEST, { {"success", false}, {"error", "Nothing was saved: fix the settings marked in red"}, {"errors", errors} });
				return;
			}
			nlohmann::json restart = nlohmann::json::array();
			for (const auto& change : changes) {
				ApplyChange(context, change);
				const auto* info = SettingsCatalog::Find(change.file, change.name);
				if (info && info->restart) restart.push_back(info->title);
			}
			ReloadEverywhere();
			BroadcastTableChanged("settings");
			JsonSuccess(reply, { {"saved", changes.size()}, {"restart", restart},
				{"message", "Saved " + std::to_string(changes.size()) + " setting" + (changes.size() == 1 ? "" : "s") + ". Servers reloaded their settings."} });
		});

	Route(eHTTPMethod::GET, "/api/settings/options", Perm("settings"), "Every value a list setting can pick from. Query: ?of=zone|reward_code",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto of = QueryValue(context.queryString, "of");
			if (of != "zone" && of != "reward_code") return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "of is zone or reward_code");
			reply.headers.push_back("Cache-Control: private, max-age=3600");
			JsonSuccess(reply, { {"options", ListOptions(of)} });
		});

	Route(eHTTPMethod::GET, "/api/settings/names", Perm("settings"), "Names for list values. Query: ?of=zone|lot|reward_code&ids=1,2,3",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto of = QueryValue(context.queryString, "of");
			if (of != "zone" && of != "lot" && of != "reward_code") return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "of is zone, lot or reward_code");
			JsonSuccess(reply, { {"names", ListNames(of, ParseIds(QueryValue(context.queryString, "ids")))} });
		});

	Route(eHTTPMethod::POST, "/api/settings/delete", Perm("settings"), "Forget a setting that only exists on the web. Body: {file, name}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string file = body->value("file", ""), name = body->value("name", "");
			if (IsPermissionSetting(name)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Change permissions on the Permissions page");
			const auto settings = Database::Get()->GetServerConfig({ file });
			const auto it = std::ranges::find_if(settings, [&](const auto& s) { return s.name == name; });
			if (it == settings.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Setting not found");
			if (!it->fileSource.empty()) return JsonError(reply, eHTTPStatusCode::CONFLICT, "This setting is in a config file; clear its web value instead");
			SettingsHistory::Record(context, file, name, *it, std::nullopt, false, true);
			Database::Get()->DeleteServerConfig(file, name);
			Audit(context, "change_setting", file + " " + name + " removed");
			ReloadEverywhere();
			BroadcastTableChanged("settings");
			JsonSuccess(reply, { {"message", "Setting removed"} });
		});

	Route(eHTTPMethod::GET, "/api/account/permissions", 0, "What you may do: {permissions: {name: bool}}",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonSuccess(reply, { {"gmLevel", context.gmLevel}, {"permissions", Permissions::ForLevel(context.gmLevel, context.apiKey.get(), context.grants.get())} });
		});

	Route(eHTTPMethod::GET, "/api/permissions", Perm("permissions_manage"), "Every permission with its default and current minimum GM level, and where that comes from",
		[](HTTPReply& reply, const HTTPContext&) {
			std::map<std::string, IServerConfig::Setting> web;
			for (auto& setting : Database::Get()->GetServerConfig({ Game::config->GetFileName() })) {
				if (IsPermissionSetting(setting.name)) web[setting.name] = std::move(setting);
			}
			nlohmann::json permissions = nlohmann::json::array();
			for (const auto& permission : Permissions::All()) {
				permissions.push_back({
					{"key", permission.key}, {"category", permission.category}, {"title", permission.title}, {"description", permission.description},
					{"default", permission.defaultLevel}, {"level", Permissions::Level(permission.key)}, {"locked", permission.locked}, {"minLevel", permission.minLevel},
					{"source", PermissionSource(permission, web)}, {"setting", Permissions::ConfigName(permission.key)}
				});
			}
			JsonSuccess(reply, { {"permissions", permissions}, {"min", Permissions::PLAYER_LEVEL}, {"max", Permissions::MAX_LEVEL} });
		});

	Route(eHTTPMethod::POST, "/api/permissions", Perm("permissions_manage"),
		"Change who may do something. Body: {key, level (1-9, or 0-9 for what players do; null goes back to the config file or default)}. Takes effect at once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto* permission = Permissions::Find(body->value("key", ""));
			if (!permission) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown permission");
			if (permission->locked) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "This permission is fixed");
			std::optional<std::string> value;
			if (body->contains("level") && !(*body)["level"].is_null()) {
				const auto& level = (*body)["level"];
				if (!level.is_number_integer() || level.get<int>() < permission->minLevel || level.get<int>() > Permissions::MAX_LEVEL) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The level must be " + std::to_string(permission->minLevel) + " to 9" +
						(permission->minLevel > 0 ? " (staff permissions can't be given to players)" : ""));
				}
				value = std::to_string(level.get<int>());
			}
			const auto before = Permissions::Level(permission->key);
			// The page's value beats the config file, like "web value wins" on the Settings page
			Database::Get()->SetWebConfigValue(Game::config->GetFileName(), Permissions::ConfigName(permission->key), value, value.has_value(), context.authenticatedUser);
			// Every server: the world servers use it for the slash commands paired with it
			ReloadEverywhere();
			const auto after = Permissions::Level(permission->key);
			std::string inGame;
			try {
				inGame = FollowingCommands(permission->key, Database::Get()->GetSlashCommands(), CommandLevelRows());
			} catch (const std::exception&) {} // no slash_commands table yet
			Audit(context, "change_permission", permission->title + " (" + permission->key + "): GM " + std::to_string(before) + "+ -> GM " +
				std::to_string(after) + "+" + (value ? "" : " (back to the file or default)") + (inGame.empty() ? "" : "; in game too: " + inGame));
			BroadcastTableChanged("permissions", permission->key);
			JsonSuccess(reply, { {"level", after}, {"message", permission->title + " now needs GM " + std::to_string(after) + (inGame.empty() ? "" : " (in game too: " + inGame + ")")} });
		});

	Route(eHTTPMethod::GET, "/api/permissions/commands", Perm("permissions_manage"),
		"Every in-game slash command the world servers registered: aliases, help, default and current minimum GM level, where that comes from, and the dashboard permission that does the same thing",
		[](HTTPReply& reply, const HTTPContext&) {
			const auto rows = CommandLevelRows();
			nlohmann::json commands = nlohmann::json::array();
			for (const auto& command : Database::Get()->GetSlashCommands()) {
				const auto current = ResolveCommandLevel(command, rows);
				nlohmann::json permission = nullptr;
				// A paired command's default is its permission's default (the code's level only shows where it came from)
				uint8_t defaultLevel = command.defaultLevel;
				if (const auto* linked = Permissions::Find(command.dashboardPermission)) {
					permission = { {"key", linked->key}, {"title", linked->title}, {"level", Permissions::Level(linked->key)}, {"setting", Permissions::ConfigName(linked->key)} };
					if (!command.fixed) defaultLevel = SlashCommandLevels::ResolvePaired(command.defaultLevel, command.minLevel, false, linked->defaultLevel, "").level;
				}
				// Who it may be used on: the self permission it needs on your own account, or none ("others": only checked on other players)
				const auto rule = SlashCommandLevels::TargetRuleFromName(command.targetRule);
				nlohmann::json target = nullptr;
				if (rule != SlashCommandLevels::eTargetRule::NONE) {
					target = { {"rule", command.targetRule}, {"equalPermission", AccountRules::EQUAL_RANK_PERMISSION} };
					switch (rule) {
					case SlashCommandLevels::eTargetRule::TOOLS: target["selfPermission"] = AccountRules::SelfPermission(AccountRules::eAccountAction::TOOLS); break;
					case SlashCommandLevels::eTargetRule::ITEMS: target["selfPermission"] = AccountRules::SelfPermission(AccountRules::eAccountAction::ITEMS); break;
					case SlashCommandLevels::eTargetRule::MODERATION: target["selfPermission"] = AccountRules::SelfPermission(AccountRules::eAccountAction::MODERATION); break;
					default: target["selfPermission"] = nullptr; break;
					}
				}
				commands.push_back({
					{"name", command.name}, {"aliases", command.aliases}, {"help", command.help}, {"info", command.info},
					{"default", defaultLevel}, {"codeLevel", command.defaultLevel}, {"level", current.level}, {"minLevel", command.minLevel}, {"fixed", command.fixed},
					{"clientHandled", command.clientHandled}, {"note", command.note}, {"source", current.source}, {"setting", SlashCommandLevels::ConfigName(command.name)}, {"permission", permission},
					{"overridden", current.overridden}, {"followSet", current.followSet}, {"localSource", current.localSource}, {"keptFromUpgrade", current.keptFromUpgrade},
					{"target", target}
				});
			}
			JsonSuccess(reply, { {"commands", commands}, {"file", SlashCommandLevels::CONFIG_FILE}, {"followValue", SlashCommandLevels::FOLLOW_PERMISSION} });
		});

	Route(eHTTPMethod::POST, "/api/permissions/commands", Perm("permissions_manage"),
		"Change who may use a slash command. Body: {name, level (its floor to 9; null goes back to the config file or default)}, or {name, follow: true} to drop the "
		"override of a command paired with a dashboard permission (it then uses the permission's level). A paired command's level is its permission's: change "
		"that instead. World servers pick it up at once",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto name = body->value("name", "");
			const auto commands = Database::Get()->GetSlashCommands();
			const auto command = std::ranges::find_if(commands, [&](const auto& c) { return c.name == name; });
			if (command == commands.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown command");
			if (command->fixed) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "This command's level is fixed" + (command->note.empty() ? "" : ": " + command->note));
			const auto* paired = Permissions::Find(command->dashboardPermission);
			const auto rows = CommandLevelRows();
			const auto current = ResolveCommandLevel(*command, rows);
			const auto alias = "/" + command->aliases.front();
			std::optional<std::string> value;
			std::string what;
			if (body->value("follow", false)) {
				if (!paired) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, alias + " isn't paired with a dashboard permission");
				// A file or environment value can't be removed from here: a value set here that says "follow the permission" beats it
				if (!current.localSource.empty()) value = std::string(SlashCommandLevels::FOLLOW_PERMISSION);
				what = " (follows " + paired->key + ")";
			} else if (body->contains("level") && !(*body)["level"].is_null()) {
				if (paired) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, alias + " uses the level of the " + paired->key + " permission (" + paired->title +
						"): change it on that permission's row" + (current.overridden ? ", or drop this command's override first" : ""));
				}
				const auto& level = (*body)["level"];
				if (!level.is_number_integer() || level.get<int>() < command->minLevel || level.get<int>() > SlashCommandLevels::MAX_LEVEL) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The level must be " + std::to_string(command->minLevel) + " to 9" +
						(command->note.empty() ? (command->minLevel > 0 ? " (staff commands can't be given to players)" : "") : " (" + command->note + ")"));
				}
				value = std::to_string(level.get<int>());
			} else {
				what = paired ? (current.localSource.empty() ? " (follows " + paired->key + ")" : " (back to the " + current.localSource + "'s override)") : " (back to the file or default)";
			}
			// Stored for the world servers, beating their config files like a permission does the dashboard's
			Database::Get()->SetWebConfigValue(std::string(SlashCommandLevels::CONFIG_FILE), SlashCommandLevels::ConfigName(command->name), value, value.has_value(), context.authenticatedUser);
			ReloadEverywhere();
			const auto after = ResolveCommandLevel(*command, CommandLevelRows()).level;
			Audit(context, "change_command_level", alias + " (" + command->name + "): GM " + std::to_string(current.level) + "+ -> GM " + std::to_string(after) + "+" + what);
			BroadcastTableChanged("permissions", "command:" + command->name);
			JsonSuccess(reply, { {"level", after}, {"message", alias + " now needs GM " + std::to_string(after) + what} });
		});
}
