#pragma once

#include <map>
#include <set>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "IServerConfig.h"

class dConfig;

/**
 * Keeps a server's config in step with the server_config table: reports what the server's files and environment
 * set, and hands back the dashboard values that apply to it.
 */
namespace ConfigSync {
	// Report this server's file values and apply the database's. Safe to call before the table exists.
	void Sync(dConfig& config);

	struct Resolved {
		std::map<std::string, std::string> overrides; // web value wins
		std::map<std::string, std::string> fallbacks; // web value used because nothing else sets the key
	};

	/**
	 * Which database values apply to a server. Rows of its own file come before sharedconfig.ini rows for the same key,
	 * matching how files are read.
	 * @param setLocally keys the server's files or environment already set
	 */
	inline Resolved Resolve(const std::vector<IServerConfig::Setting>& rows, const std::string& ownFile, const std::set<std::string>& setLocally) {
		Resolved resolved;
		std::set<std::string> decided;
		for (const bool own : { true, false }) {
			for (const auto& row : rows) {
				if ((row.file == ownFile) != own || !row.webValue || decided.contains(row.name)) continue;
				decided.insert(row.name);
				if (row.webWins) resolved.overrides[row.name] = *row.webValue;
				else if (!setLocally.contains(row.name)) resolved.fallbacks[row.name] = *row.webValue;
			}
		}
		return resolved;
	}

	/**
	 * Settings one server owns that the others read too: the dashboard's permission_* levels, which the world servers
	 * need for their slash commands. Rows are the owner's (file = the owner's config file), as it reported and stored
	 * them. The result replaces what the reading server would otherwise use for those keys, in the owner's order: a web
	 * value that wins, the owner's file or environment, then (only when the reader's own files leave the key out, e.g.
	 * sharedconfig.ini, which both read) any other web value.
	 */
	inline void AddOwnedValues(Resolved& resolved, const std::vector<IServerConfig::Setting>& ownerRows, const std::string& ownerFile, std::string_view prefix) {
		for (const auto& row : ownerRows) {
			if (row.file != ownerFile || !row.name.starts_with(prefix)) continue;
			std::optional<std::string> value;
			bool fallback = false;
			if (row.webWins && row.webValue) value = row.webValue;
			else if (!row.fileSource.empty() && row.fileValue && !row.fileValue->empty()) value = row.fileValue;
			else if (row.webValue && !row.webValue->empty()) { value = row.webValue; fallback = true; }
			if (!value) continue;
			resolved.overrides.erase(row.name);
			resolved.fallbacks.erase(row.name);
			(fallback ? resolved.fallbacks : resolved.overrides)[row.name] = *value;
		}
	}
}
