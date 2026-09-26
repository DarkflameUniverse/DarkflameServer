#include "ConfigSync.h"

#include "Database.h"
#include "Logger.h"
#include "dConfig.h"
#include "Permissions.h"

namespace ConfigSync {
	namespace {
		/**
		 * The dashboard reports every permission level its own file or environment sets, so the other servers can use
		 * them: environment variables for keys that aren't in the file too, and a key taken out of the file (or the
		 * environment) is cleared rather than left at its old value.
		 */
		void ReportPermissionLevels(dConfig& config) {
			std::set<std::string> inFile;
			for (const auto& entry : config.GetFileEntries()) {
				if (entry.file == config.GetFileName()) inFile.insert(entry.key);
			}
			std::map<std::string, IServerConfig::Setting> stored;
			for (auto& row : Database::Get()->GetServerConfig({ config.GetFileName() })) {
				if (Permissions::IsPermissionSetting(row.name)) stored[row.name] = std::move(row);
			}
			for (const auto& permission : Permissions::All()) {
				const auto name = Permissions::ConfigName(permission.key);
				if (inFile.contains(name)) continue; // reported with the file's entries
				const auto env = dConfig::GetEnvironmentValue(name);
				const auto it = stored.find(name);
				const bool reported = it != stored.end() && !it->second.fileSource.empty();
				if (!env && !reported) continue;
				if (env && reported && it->second.fileSource == "env" && it->second.fileValue == env) continue;
				IServerConfig::Setting setting;
				setting.file = config.GetFileName();
				setting.name = name;
				setting.fileSource = env ? "env" : "";
				if (env) setting.fileValue = *env;
				Database::Get()->ReportConfigFromFile(setting);
			}
		}
	}

	void Sync(dConfig& config) {
		try {
			std::set<std::string> setLocally;
			for (const auto& entry : config.GetFileEntries()) {
				if (dConfig::IsFileOnlyKey(entry.key)) continue;
				const auto env = dConfig::GetEnvironmentValue(entry.key);
				const bool secret = dConfig::IsSecretKey(entry.key);
				setLocally.insert(entry.key);
				IServerConfig::Setting setting;
				setting.file = entry.file;
				setting.name = entry.key;
				setting.fileSource = env ? "env" : "file";
				// Secrets stay where they were put; the dashboard only learns that they are set
				if (!secret) setting.fileValue = env ? *env : entry.value;
				setting.secret = secret;
				setting.description = entry.description;
				Database::Get()->ReportConfigFromFile(setting);
			}

			const auto rows = Database::Get()->GetServerConfig({ config.GetFileName(), "sharedconfig.ini" });
			// Keys set only by environment variables count as set locally too
			for (const auto& row : rows) {
				if (!setLocally.contains(row.name) && dConfig::GetEnvironmentValue(row.name)) setLocally.insert(row.name);
			}
			auto resolved = Resolve(rows, config.GetFileName(), setLocally);

			// Permission levels belong to the dashboard; every other server uses the values it stored
			const std::string permissionFile(Permissions::CONFIG_FILE);
			if (config.GetFileName() == permissionFile) {
				ReportPermissionLevels(config);
			} else {
				AddOwnedValues(resolved, Database::Get()->GetServerConfig({ permissionFile }), permissionFile, Permissions::PREFIX);
			}
			config.SetDatabaseValues(std::move(resolved.overrides), std::move(resolved.fallbacks));
		} catch (const std::exception& ex) {
			// Before the migration has run there is no table yet; the files alone are used
			LOG_DEBUG("Settings from the database are not available: %s", ex.what());
		}
	}
}
