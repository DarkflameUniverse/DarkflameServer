#include "UgcStorage.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <random>

#include "ZCompression.h"

namespace {
	constexpr std::array KNOWN_FILES = {
		"model.nif", "model.nif.gz", "model.nif.checksum", "model.nif.sd0",
		"model.lxfml.gz", "model.lxfml.checksum",
		"icon.dds.gz", "icon.dds.checksum", "icon.dds.sd0", "icon.png",
		"model.noao.nif", "model.noao.nif.gz", "stats.json", "combo.json",
		"previous.icon.png", "previous.model.nif.gz", "previous.stats.json",
	};

	// What of the files made before is kept when an item is made again, for comparing (as previous.<name>)
	constexpr std::array KEPT_FILES = { "icon.png", "model.nif.gz", "stats.json" };

	const char* KindFolder(UgcStorage::Kind kind) {
		return kind == UgcStorage::Kind::MODEL ? "models" : "modular";
	}

	std::string RandomSuffix() {
		static thread_local std::mt19937_64 random{ std::random_device{}() };
		return std::to_string(random());
	}
}

UgcStorage::UgcStorage(std::filesystem::path root) : m_Root(std::move(root)) {}

bool UgcStorage::IsKnownFile(const std::string& name) {
	return std::find(KNOWN_FILES.begin(), KNOWN_FILES.end(), name) != KNOWN_FILES.end();
}

std::filesystem::path UgcStorage::Folder(Kind kind, LWOOBJID id) const {
	const auto id64 = static_cast<uint64_t>(id);
	return m_Root / KindFolder(kind) / std::to_string(id64 % 1000) / std::to_string(id64);
}

std::optional<std::filesystem::path> UgcStorage::File(Kind kind, LWOOBJID id, const std::string& name) const {
	if (!IsKnownFile(name)) return std::nullopt;
	auto path = Folder(kind, id) / name;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error)) return std::nullopt;
	return path;
}

std::optional<uint64_t> UgcStorage::Write(Kind kind, LWOOBJID id, const Files& files, std::string& error) const {
	const auto folder = Folder(kind, id);
	const auto temporary = folder.parent_path() / (".tmp-" + folder.filename().string() + "-" + RandomSuffix());
	std::error_code code;
	std::filesystem::create_directories(temporary, code);
	if (code) {
		error = "could not create " + temporary.string() + ": " + code.message();
		return std::nullopt;
	}
	uint64_t bytes = 0;
	for (const auto& [name, data] : files) {
		std::ofstream out(temporary / name, std::ios::binary | std::ios::trunc);
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
		if (!out) {
			error = "could not write " + (temporary / name).string();
			std::filesystem::remove_all(temporary, code);
			return std::nullopt;
		}
		bytes += data.size();
	}
	// Keep the last version's previews to compare with
	for (const auto* name : KEPT_FILES) {
		if (files.contains(name)) {
			std::filesystem::copy_file(folder / name, temporary / (std::string("previous.") + name), std::filesystem::copy_options::overwrite_existing, code);
			if (!code) bytes += std::filesystem::file_size(temporary / (std::string("previous.") + name), code);
			code.clear();
		}
	}
	// Swap the old folder out and the new one in; the old one is deleted after
	const auto old = folder.parent_path() / (".old-" + folder.filename().string() + "-" + RandomSuffix());
	const bool hadOld = std::filesystem::exists(folder, code);
	if (hadOld) std::filesystem::rename(folder, old, code);
	std::filesystem::rename(temporary, folder, code);
	if (code) {
		error = "could not move the files into " + folder.string() + ": " + code.message();
		std::filesystem::remove_all(temporary, code);
		return std::nullopt;
	}
	if (hadOld) std::filesystem::remove_all(old, code);
	return bytes;
}

std::optional<uint64_t> UgcStorage::Update(Kind kind, LWOOBJID id, const Files& files, std::string& error) const {
	const auto folder = Folder(kind, id);
	std::error_code code;
	if (!std::filesystem::is_directory(folder, code)) {
		error = "no files to update in " + folder.string();
		return std::nullopt;
	}
	uint64_t bytes = 0;
	for (const auto* name : KEPT_FILES) {
		if (files.contains(name) && std::filesystem::exists(folder / name, code)) {
			std::filesystem::copy_file(folder / name, folder / (std::string("previous.") + name), std::filesystem::copy_options::overwrite_existing, code);
			code.clear();
		}
	}
	for (const auto& [name, data] : files) {
		if (!IsKnownFile(name)) continue;
		const auto aside = folder / (".tmp-" + name + "-" + RandomSuffix());
		{
			std::ofstream out(aside, std::ios::binary | std::ios::trunc);
			out.write(data.data(), static_cast<std::streamsize>(data.size()));
			if (!out) {
				error = "could not write " + aside.string();
				std::filesystem::remove(aside, code);
				return std::nullopt;
			}
		}
		std::filesystem::rename(aside, folder / name, code);
		if (code) {
			error = "could not replace " + (folder / name).string() + ": " + code.message();
			std::filesystem::remove(aside, code);
			return std::nullopt;
		}
		bytes += data.size();
	}
	return bytes;
}

void UgcStorage::Remove(Kind kind, LWOOBJID id) const {
	std::error_code error;
	std::filesystem::remove_all(Folder(kind, id), error);
}

void UgcStorage::Touch(Kind kind, LWOOBJID id) const {
	std::error_code error;
	std::filesystem::last_write_time(Folder(kind, id), std::filesystem::file_time_type::clock::now(), error);
}

std::vector<UgcStorage::Entry> UgcStorage::List() const {
	std::vector<Entry> entries;
	std::error_code error;
	for (const auto kind : { Kind::MODEL, Kind::MODULAR }) {
		const auto base = m_Root / KindFolder(kind);
		for (std::filesystem::directory_iterator bucket(base, error), end; !error && bucket != end; bucket.increment(error)) {
			if (!bucket->is_directory(error)) continue;
			for (std::filesystem::directory_iterator item(bucket->path(), error), itemEnd; !error && item != itemEnd; item.increment(error)) {
				const auto name = item->path().filename().string();
				if (name.empty() || name[0] == '.' || !item->is_directory(error)) continue;
				Entry entry;
				entry.kind = kind;
				try {
					entry.id = static_cast<LWOOBJID>(std::stoull(name));
				} catch (...) {
					continue;
				}
				entry.used = std::filesystem::last_write_time(item->path(), error);
				for (std::filesystem::directory_iterator file(item->path(), error), fileEnd; !error && file != fileEnd; file.increment(error)) {
					if (file->is_regular_file(error)) entry.bytes += file->file_size(error);
				}
				entries.push_back(entry);
			}
			error.clear();
		}
		error.clear();
	}
	return entries;
}

std::vector<UgcStorage::Entry> UgcStorage::Evict(uint64_t maxBytes) const {
	auto entries = List();
	uint64_t total = 0;
	for (const auto& entry : entries) total += entry.bytes;
	std::vector<Entry> removed;
	if (total <= maxBytes) return removed;
	std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.used < b.used; });
	for (const auto& entry : entries) {
		if (total <= maxBytes) break;
		Remove(entry.kind, entry.id);
		total -= std::min(total, entry.bytes);
		removed.push_back(entry);
	}
	return removed;
}

std::optional<std::string> UgcStorage::ReadNif(Kind kind, LWOOBJID id, const std::string& name) const {
	const auto read = [](const std::filesystem::path& path) -> std::optional<std::string> {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::nullopt;
		return std::string(std::istreambuf_iterator<char>(in), {});
	};
	if (const auto packed = File(kind, id, name + ".gz")) {
		const auto data = read(*packed);
		return data ? ZCompression::Gunzip(*data) : std::nullopt;
	}
	if (const auto plain = File(kind, id, name)) return read(*plain);
	return std::nullopt;
}
