#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/**
 * What a dashboard API key may do. A key never does more than the account that owns it: every check is the owner's
 * permission right now (their GM level, the self and rank rules) AND the key's scope. Demoting or banning the owner
 * narrows or stops their keys at once, since the owner is looked up on every request.
 */
namespace ApiKeys {
	// Keys are "dlk_" followed by 64 hex characters; the dashboard's other bearer tokens (JWTs) start with "eyJ"
	constexpr std::string_view TOKEN_PREFIX = "dlk_";
	// Stored in place of a permission list: every permission the owner has, whatever they are at the time
	constexpr std::string_view ALL_PERMISSIONS = "*";

	struct Scope {
		uint64_t keyId{};
		std::string name;
		bool allPermissions{};
		std::set<std::string> permissions;
		bool readOnly{};        // GET, HEAD and OPTIONS only

		// Whether the key's scope names a permission (the owner must still have it)
		bool Has(const std::string& permission) const { return allPermissions || permissions.contains(permission); }
	};

	// "*" or a comma-separated list, as stored in the database
	inline std::string JoinPermissions(bool all, const std::set<std::string>& permissions) {
		if (all) return std::string(ALL_PERMISSIONS);
		std::string out;
		for (const auto& permission : permissions) {
			if (!out.empty()) out += ',';
			out += permission;
		}
		return out;
	}

	inline void ParsePermissions(std::string_view text, bool& all, std::set<std::string>& permissions) {
		all = text == ALL_PERMISSIONS;
		permissions.clear();
		if (all) return;
		size_t start = 0;
		while (start <= text.size()) {
			const auto end = std::min(text.find(',', start), text.size());
			if (end > start) permissions.emplace(text.substr(start, end - start));
			start = end + 1;
		}
	}

	// Comma-separated values (IP addresses, path prefixes), trimmed, empty entries dropped
	inline std::vector<std::string> SplitList(std::string_view text) {
		std::vector<std::string> out;
		size_t start = 0;
		while (start <= text.size()) {
			const auto end = std::min(text.find(',', start), text.size());
			auto item = text.substr(start, end - start);
			while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.remove_prefix(1);
			while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.remove_suffix(1);
			if (!item.empty()) out.emplace_back(item);
			start = end + 1;
		}
		return out;
	}

	/**
	 * Whether a client address matches a key's allowed list: an exact address, or a prefix ending in '.' or ':'
	 * ("10.0.0." matches 10.0.0.x). An empty list allows every address.
	 */
	inline bool AddressAllowed(const std::vector<std::string>& allowed, std::string_view address) {
		if (allowed.empty()) return true;
		for (const auto& entry : allowed) {
			if (entry == address) return true;
			if ((entry.back() == '.' || entry.back() == ':') && address.starts_with(entry)) return true;
		}
		return false;
	}

	// Whether a (lowercased) request path is inside a key's allowed path prefixes. An empty list allows every path.
	inline bool PathAllowed(const std::vector<std::string>& prefixes, std::string_view path) {
		if (prefixes.empty()) return true;
		for (const auto& prefix : prefixes) {
			if (path.starts_with(prefix)) return true;
		}
		return false;
	}
}
