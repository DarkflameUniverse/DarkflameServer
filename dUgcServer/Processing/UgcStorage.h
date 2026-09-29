#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Where the UGC server keeps what it makes: <root>/models/<id % 1000>/<id>/ for player models and
 * <root>/modular/<id % 1000>/<id>/ for modular builds. An item's files are written to a temporary folder and renamed
 * into place, so a request never sees half of them. Plain file operations, safe from any thread for different items.
 */
class UgcStorage {
public:
	enum class Kind { MODEL, MODULAR };

	// The files an item has, by name (see FileName)
	using Files = std::map<std::string, std::string>;

	explicit UgcStorage(std::filesystem::path root);

	const std::filesystem::path& GetRoot() const { return m_Root; }

	std::filesystem::path Folder(Kind kind, LWOOBJID id) const;

	// A file of an item, if it is there; `name` must be one of the names the server writes
	std::optional<std::filesystem::path> File(Kind kind, LWOOBJID id, const std::string& name) const;

	// Replaces an item's files with `files`; the bytes written, nullopt (and `error`) on failure
	std::optional<uint64_t> Write(Kind kind, LWOOBJID id, const Files& files, std::string& error) const;

	// Replaces some of an item's files in place (each written aside and renamed over the old one), keeping the others;
	// the ones kept for comparing (icon.png...) are copied to previous.* first. The bytes written, nullopt when the item
	// has no folder or a write fails.
	std::optional<uint64_t> Update(Kind kind, LWOOBJID id, const Files& files, std::string& error) const;

	// Deletes an item's files
	void Remove(Kind kind, LWOOBJID id) const;

	// Marks an item used now, for eviction
	void Touch(Kind kind, LWOOBJID id) const;

	struct Entry {
		Kind kind{};
		LWOOBJID id{};
		uint64_t bytes{};
		std::filesystem::file_time_type used{};
	};

	// Everything stored, with sizes and when it was last used
	std::vector<Entry> List() const;

	// Removes the least recently used items until the total is at most `maxBytes`; what was removed
	std::vector<Entry> Evict(uint64_t maxBytes) const;

	// An item's .nif (`name` model.nif, model.noao.nif or previous.model.nif): stored compressed (<name>.gz), or as it is
	// (made by older versions); nullopt when there's neither
	std::optional<std::string> ReadNif(Kind kind, LWOOBJID id, const std::string& name) const;

	// Names the server writes (and serves); anything else is refused
	static bool IsKnownFile(const std::string& name);

private:
	std::filesystem::path m_Root;
};
