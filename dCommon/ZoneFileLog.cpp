#include "ZoneFileLog.h"

#include <algorithm>

#include "AssetManager.h"
#include "FdbSnapshot.h"

namespace {
	std::vector<ZoneFileLog::Entry> g_Entries;

	std::string Absolute(const std::filesystem::path& path) {
		std::error_code code;
		const auto absolute = std::filesystem::absolute(path, code);
		return (code ? path : absolute).lexically_normal().generic_string();
	}
}

const char* ZoneFileLog::KindName(eKind kind) {
	switch (kind) {
	case eKind::ZONE: return "zone";
	case eKind::SCENE: return "scene";
	case eKind::TRIGGERS: return "triggers";
	case eKind::TERRAIN: return "terrain";
	case eKind::NAVMESH: return "navmesh";
	case eKind::OTHER: return "other";
	}
	return "other";
}

void ZoneFileLog::Record(eKind kind, const std::string& path, bool packed, std::string_view bytes) {
	Entry entry;
	entry.kind = kind;
	entry.packed = packed;
	entry.size = bytes.size();
	entry.hash = FdbSnapshot::Hash(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
	entry.path = path;
	const auto existing = std::find_if(g_Entries.begin(), g_Entries.end(), [&path](const Entry& e) { return e.path == path; });
	if (existing != g_Entries.end()) *existing = std::move(entry);
	else g_Entries.push_back(std::move(entry));
}

void ZoneFileLog::RecordAsset(const AssetManager* assets, eKind kind, const std::string& name, const AssetStream& stream) {
	const auto loose = assets ? assets->GetLoosePath(name) : std::nullopt;
	Record(kind, loose ? Absolute(*loose) : name, !loose, stream.Bytes());
}

void ZoneFileLog::RecordFile(eKind kind, const std::filesystem::path& path) {
	std::error_code code;
	const auto size = std::filesystem::file_size(path, code);
	if (code) return;
	const auto hash = FdbSnapshot::HashFile(path);
	if (!hash) return;
	Entry entry;
	entry.kind = kind;
	entry.size = static_cast<uint64_t>(size);
	entry.hash = *hash;
	entry.path = Absolute(path);
	const auto existing = std::find_if(g_Entries.begin(), g_Entries.end(), [&entry](const Entry& e) { return e.path == entry.path; });
	if (existing != g_Entries.end()) *existing = std::move(entry);
	else g_Entries.push_back(std::move(entry));
}

const std::vector<ZoneFileLog::Entry>& ZoneFileLog::Entries() {
	return g_Entries;
}

void ZoneFileLog::Clear() {
	g_Entries.clear();
}
