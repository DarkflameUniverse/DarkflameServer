#include "FdbSnapshot.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>

#include "FdbReader.h"

namespace {
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;
	constexpr const char* FDB_PREFIX = "cdclient-";
	constexpr const char* FDB_SUFFIX = ".fdb";
	constexpr const char* SQLITE_PREFIX = "CDServer-";
	constexpr const char* SQLITE_SUFFIX = ".sqlite";
	constexpr const char* TEMP_SUFFIX = ".tmp";

	bool StartsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

	bool EndsWith(const std::string& text, const char* suffix) {
		const auto length = std::strlen(suffix);
		return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
	}

	std::string TempName(const std::string& base) {
		std::random_device random;
		const uint64_t salt = (static_cast<uint64_t>(random()) << 32) ^ random();
		return base + "." + FdbSnapshot::HashText(salt) + TEMP_SUFFIX;
	}

	uint64_t HashRow(const FdbReader::Row& row) {
		uint64_t hash = 14695981039346656037ULL;
		const auto mix = [&hash](const void* data, uint64_t size) { hash = FdbSnapshot::Hash(static_cast<const uint8_t*>(data), size, hash); };
		for (uint32_t c = 0; c < row.GetFieldCount(); c++) {
			const auto type = static_cast<uint32_t>(row.GetType(c));
			mix(&type, sizeof(type));
			switch (row.GetType(c)) {
			case eSqliteDataType::INT32:
			case eSqliteDataType::INT_BOOL:
			case eSqliteDataType::INT64: {
				const int64_t value = row.GetInt64(c);
				mix(&value, sizeof(value));
				break;
			}
			case eSqliteDataType::REAL: {
				const float value = row.GetFloat(c);
				mix(&value, sizeof(value));
				break;
			}
			case eSqliteDataType::TEXT_4:
			case eSqliteDataType::TEXT_8: {
				const auto text = row.GetRawString(c);
				const auto length = static_cast<uint64_t>(text.size());
				mix(text.data(), text.size());
				mix(&length, sizeof(length));
				break;
			}
			default:
				break;
			}
		}
		// splitmix64 finish so the per-table sum doesn't cancel out by accident
		uint64_t z = hash + 0x9E3779B97F4A7C15ULL;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
}

uint64_t FdbSnapshot::Hash(const uint8_t* data, uint64_t size, uint64_t seed) {
	uint64_t hash = seed;
	for (uint64_t i = 0; i < size; i++) {
		hash ^= data[i];
		hash *= FNV_PRIME;
	}
	return hash;
}

std::optional<uint64_t> FdbSnapshot::HashFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return std::nullopt;
	std::vector<char> buffer(1 << 20);
	uint64_t hash = 14695981039346656037ULL;
	while (file) {
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto read = file.gcount();
		if (read <= 0) break;
		hash = Hash(reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<uint64_t>(read), hash);
	}
	if (file.bad()) return std::nullopt;
	return hash;
}

std::string FdbSnapshot::HashText(uint64_t hash) {
	std::array<char, 17> text{};
	std::snprintf(text.data(), text.size(), "%016" PRIx64, hash);
	return text.data();
}

std::string FdbSnapshot::FdbName(uint64_t hash) {
	return FDB_PREFIX + HashText(hash) + FDB_SUFFIX;
}

std::string FdbSnapshot::SqliteName(uint64_t hash) {
	return SQLITE_PREFIX + HashText(hash) + SQLITE_SUFFIX;
}

std::optional<uint64_t> FdbSnapshot::ParseName(const std::string& name) {
	std::string hex;
	if (StartsWith(name, FDB_PREFIX) && EndsWith(name, FDB_SUFFIX)) {
		hex = name.substr(std::strlen(FDB_PREFIX), name.size() - std::strlen(FDB_PREFIX) - std::strlen(FDB_SUFFIX));
	} else if (StartsWith(name, SQLITE_PREFIX) && EndsWith(name, SQLITE_SUFFIX)) {
		hex = name.substr(std::strlen(SQLITE_PREFIX), name.size() - std::strlen(SQLITE_PREFIX) - std::strlen(SQLITE_SUFFIX));
	} else {
		return std::nullopt;
	}
	if (hex.size() != 16) return std::nullopt;
	uint64_t hash = 0;
	for (const char c : hex) {
		uint64_t digit = 0;
		if (c >= '0' && c <= '9') digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else return std::nullopt;
		hash = (hash << 4) | digit;
	}
	return hash;
}

std::optional<uint64_t> FdbSnapshot::MakeCopy(const std::filesystem::path& source, const std::filesystem::path& dir, std::string& error) {
	std::error_code code;
	std::filesystem::create_directories(dir, code);
	const auto temp = dir / TempName(FDB_PREFIX);
	if (!std::filesystem::copy_file(source, temp, std::filesystem::copy_options::overwrite_existing, code)) {
		error = "could not copy " + source.string() + ": " + code.message();
		std::filesystem::remove(temp, code);
		return std::nullopt;
	}

	const auto hash = HashFile(temp);
	if (!hash) {
		error = "could not read the copy " + temp.string();
		std::filesystem::remove(temp, code);
		return std::nullopt;
	}

	const auto target = dir / FdbName(*hash);
	if (std::filesystem::exists(target, code)) {
		// Same bytes already there, maybe mapped by a server; keep it
		std::filesystem::remove(temp, code);
		return hash;
	}

	std::filesystem::rename(temp, target, code);
	if (code) {
		error = "could not rename the copy to " + target.string() + ": " + code.message();
		std::filesystem::remove(temp, code);
		return std::nullopt;
	}
	return hash;
}

std::optional<FdbSnapshot::Current> FdbSnapshot::ReadCurrent(const std::filesystem::path& dir) {
	std::ifstream file(dir / CURRENT_FILE);
	if (!file) return std::nullopt;
	Current current;
	if (!std::getline(file, current.fdb) || !std::getline(file, current.sqlite)) return std::nullopt;
	for (auto* line : { &current.fdb, &current.sqlite }) {
		while (!line->empty() && (line->back() == '\r' || line->back() == ' ')) line->pop_back();
		// Names only, never a path out of resServer
		if (line->empty() || line->find_first_of("/\\") != std::string::npos || *line == "..") return std::nullopt;
	}
	return current;
}

FdbSnapshot::Resolved FdbSnapshot::Resolve(const std::filesystem::path& dir) {
	Resolved resolved{ dir / DEFAULT_SQLITE, {} };
	const auto current = ReadCurrent(dir);
	if (!current) return resolved;
	std::error_code code;
	if (!std::filesystem::is_regular_file(dir / current->sqlite, code) || !std::filesystem::is_regular_file(dir / current->fdb, code)) return resolved;
	resolved.sqlite = dir / current->sqlite;
	resolved.fdb = dir / current->fdb;
	return resolved;
}

bool FdbSnapshot::WriteCurrent(const std::filesystem::path& dir, const Current& current) {
	const auto temp = dir / TempName(CURRENT_FILE);
	{
		std::ofstream file(temp, std::ios::trunc);
		if (!file) return false;
		file << current.fdb << '\n' << current.sqlite << '\n';
		if (!file) return false;
	}
	std::error_code code;
	std::filesystem::rename(temp, dir / CURRENT_FILE, code);
	if (code) {
		std::filesystem::remove(temp, code);
		return false;
	}
	return true;
}

std::vector<std::string> FdbSnapshot::RemoveOld(const std::filesystem::path& dir, const std::set<uint64_t>& keep) {
	std::vector<std::string> removed;
	std::error_code code;
	for (const auto& entry : std::filesystem::directory_iterator(dir, code)) {
		if (!entry.is_regular_file(code)) continue;
		const auto name = entry.path().filename().string();
		bool remove = false;
		if (EndsWith(name, TEMP_SUFFIX)) {
			remove = StartsWith(name, FDB_PREFIX) || StartsWith(name, SQLITE_PREFIX) || StartsWith(name, CURRENT_FILE);
		} else if (const auto hash = ParseName(name)) {
			remove = !keep.contains(*hash);
		}
		if (!remove) continue;
		std::error_code removeCode;
		if (std::filesystem::remove(entry.path(), removeCode)) removed.push_back(name);
	}
	return removed;
}

FdbSnapshot::Stamp FdbSnapshot::StampOf(const std::filesystem::path& path) {
	Stamp stamp;
	std::error_code code;
	const auto size = std::filesystem::file_size(path, code);
	if (code) return stamp;
	const auto time = std::filesystem::last_write_time(path, code);
	if (code) return stamp;
	stamp.exists = true;
	stamp.size = static_cast<uint64_t>(size);
	stamp.mtime = static_cast<int64_t>(time.time_since_epoch().count());
	return stamp;
}

bool FdbSnapshot::Watcher::Poll(const Stamp& stamp) {
	const bool settled = stamp == m_Last;
	m_Last = stamp;
	// A missing file is never a new version; it is likely being replaced
	return stamp.exists && settled && stamp != m_Accepted;
}

std::map<std::string, FdbSnapshot::TableSummary> FdbSnapshot::Summarize(const std::filesystem::path& fdb) {
	std::map<std::string, TableSummary> summary;
	FdbReader reader;
	if (!reader.Open(fdb)) return summary;
	for (const auto& table : reader.GetTables()) {
		auto& entry = summary[table.GetName()];
		table.ForEachRow([&entry](const FdbReader::Row& row) {
			entry.rows++;
			entry.hash += HashRow(row);
		});
	}
	return summary;
}

std::vector<std::string> FdbSnapshot::DescribeChanges(const std::map<std::string, TableSummary>& before, const std::map<std::string, TableSummary>& after) {
	std::vector<std::string> lines;
	for (const auto& [name, now] : after) {
		const auto old = before.find(name);
		if (old == before.end()) {
			lines.push_back(name + ": new table, " + std::to_string(now.rows) + " rows");
		} else if (old->second.rows != now.rows || old->second.hash != now.hash) {
			lines.push_back(name + ": " + std::to_string(old->second.rows) + " -> " + std::to_string(now.rows) + " rows" +
				(old->second.rows == now.rows ? " (values changed)" : ""));
		}
	}
	for (const auto& [name, old] : before) {
		if (!after.contains(name)) lines.push_back(name + ": removed, had " + std::to_string(old.rows) + " rows");
	}
	return lines;
}
