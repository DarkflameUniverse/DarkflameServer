#include "SettingsHistory.h"
#include "SettingsCatalog.h"
#include "SettingsRoutes.h"
#include "SlashCommandLevels.h"

#include <ctime>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Logger.h"
#include "dConfig.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	// permission_* and command_level_* are changed on the Permissions page; its own routes don't write this history
	bool IsPermissionSetting(const std::string& name) {
		return name.starts_with("permission_") || SlashCommandLevels::IsLevelSetting(name);
	}

	nlohmann::json ChangeJson(const IServerConfig::SettingChange& change) {
		const auto* info = SettingsCatalog::Find(change.file, change.name);
		const auto optional = [](const std::optional<std::string>& value) { return value ? nlohmann::json(*value) : nlohmann::json(nullptr); };
		return {
			{"id", change.id}, {"file", change.file}, {"name", change.name}, {"title", info ? info->title : change.name},
			{"oldValue", optional(change.oldValue)}, {"oldWebWins", change.oldWebWins}, {"newValue", optional(change.newValue)}, {"newWebWins", change.newWebWins},
			{"fileValue", optional(change.fileValue)}, {"default", info ? info->defaultValue : ""}, {"secret", change.secret}, {"removed", change.removed},
			{"revertOf", change.revertOf}, {"changedAt", change.changedAt}, {"changedBy", change.changedBy}, {"restart", info && info->restart}
		};
	}
}

namespace SettingsHistory {
	std::optional<IServerConfig::Setting> Current(const std::string& file, const std::string& name) {
		for (auto& row : Database::Get()->GetServerConfig({ file })) {
			if (row.name == name) return std::move(row);
		}
		return std::nullopt;
	}

	void Record(const HTTPContext& context, const std::string& file, const std::string& name, const std::optional<IServerConfig::Setting>& before,
		const std::optional<std::string>& value, bool webWins, bool removed, uint64_t revertOf) {
		const std::optional<std::string> oldValue = before ? before->webValue : std::nullopt;
		const bool oldWins = before && before->webWins;
		if (!removed && !Changes(oldValue, oldWins, value, webWins)) return;
		const auto* info = SettingsCatalog::Find(file, name);
		const bool secret = dConfig::IsSecretKey(name) || (before && before->secret) || (info && info->type == SettingsCatalog::eType::SECRET);
		IServerConfig::SettingChange change;
		change.file = file;
		change.name = name;
		// Secrets are never copied out of server_config
		if (!secret) {
			change.oldValue = oldValue;
			change.newValue = value;
			change.fileValue = before && !before->fileSource.empty() ? before->fileValue : std::nullopt;
		}
		change.oldWebWins = oldWins;
		change.newWebWins = value.has_value() && webWins;
		change.secret = secret;
		change.removed = removed;
		change.revertOf = revertOf;
		change.changedAt = static_cast<int64_t>(std::time(nullptr));
		change.accountId = context.accountId;
		change.changedBy = context.authenticatedUser;
		try {
			Database::Get()->InsertSettingChange(change);
		} catch (const std::exception& ex) {
			LOG("Could not record the change of %s %s in the setting history: %s", file.c_str(), name.c_str(), ex.what());
		}
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/settings/history", Perm("settings"), "Setting history page: every change made on the dashboard, with undo",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "settings_history.jinja2", "settings_history"); });

		Route(eHTTPMethod::POST, "/api/settings/history", Perm("settings"),
			"Setting changes made on the dashboard, newest first (DataTables): old and new web value, the file value then, who and when. Body adds {file, name} for one setting",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto request = ParseDataTablesRequest(context.body);
				const auto body = ParseBody(context);
				if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string file = body->value("file", ""), name = body->value("name", "");
				if (file.empty() != name.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send both file and name, or neither");
				uint64_t total = 0;
				nlohmann::json data = nlohmann::json::array();
				for (const auto& change : Database::Get()->GetSettingChanges(file, name, request->start, std::min<uint32_t>(request->length, 200), total)) {
					data.push_back(ChangeJson(change));
				}
				JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} });
			});

		Route(eHTTPMethod::POST, "/api/settings/history/:id/revert", Perm("settings"),
			"Undo a setting change: puts back the web value it replaced, through the normal save (checked, audited, recorded, servers reloaded). "
			"Refused if the setting was changed again since",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint64_t>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid change id");
				const auto change = Database::Get()->GetSettingChange(*id);
				if (!change) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Change not found");
				if (IsPermissionSetting(change->name)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Change permissions on the Permissions page");
				std::string error;
				const auto body = RevertBody(*change, error);
				if (!body) return JsonError(reply, eHTTPStatusCode::CONFLICT, error);
				const auto current = Current(change->file, change->name);
				if (!StillCurrent(*change, current ? current->webValue : std::nullopt, current && current->webWins)) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "It has been changed again since; undo the newer change first");
				}
				if (const auto refused = SaveSetting(context, *body, *id)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *refused);
				Audit(context, "revert_setting", change->file + " " + change->name + ": undid change #" + std::to_string(*id));
				const auto* info = SettingsCatalog::Find(change->file, change->name);
				JsonSuccess(reply, { {"message", info && info->restart ? "Undone. Restart the servers for this one to take effect." : "Undone. Servers reloaded their settings."} });
			});
	}
}
