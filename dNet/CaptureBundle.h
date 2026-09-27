#ifndef __CAPTUREBUNDLE__H__
#define __CAPTUREBUNDLE__H__

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"
#include "PacketRecord.h"

/**
 * The packet bundle file format (docs/CaptureReplay.md), shared by the dashboard's capture files, exported bundles,
 * bundles converted from other captures, and the capture tool:
 *
 *   "DLUBNDL1"                 8 bytes, the format and its version
 *   u32 metadata length        little endian
 *   metadata                   UTF-8 JSON (see docs/CaptureReplay.md: origin, server version, zones and checksums,
 *                              the setup section, how ids are written)
 *   records                    PacketRecordHeader + bytes, one after another to the end of the file
 *
 * The dashboard appends records to a capture's file while it runs (one write per batch), so the metadata is written
 * once, when the file is made.
 */
namespace CaptureBundle {
	constexpr std::string_view MAGIC{ "DLUBNDL1", 8 };
	constexpr uint32_t FORMAT_VERSION = 1;
	constexpr uint32_t MAX_METADATA = 64 * 1024 * 1024;

	struct Record {
		PacketRecordHeader header;
		std::string bytes;
	};

	struct Bundle {
		nlohmann::json meta = nlohmann::json::object();
		std::vector<Record> records;
	};

	inline std::string Header(const nlohmann::json& meta) {
		const auto text = meta.dump();
		std::string out(MAGIC);
		const auto length = static_cast<uint32_t>(text.size());
		out.append(reinterpret_cast<const char*>(&length), sizeof(length));
		out += text;
		return out;
	}

	inline void AppendRecord(std::string& out, const Record& record) {
		auto header = record.header;
		header.length = static_cast<uint32_t>(record.bytes.size());
		PacketRecord::Append(out, header, record.bytes.data());
	}

	// Parses a whole file's contents. A file cut short in its last record (a capture still being written) keeps the
	// records before it; `error` is set for anything else.
	inline bool Parse(std::string_view data, Bundle& bundle, std::string& error, bool* truncated = nullptr) {
		if (data.size() < MAGIC.size() + 4 || data.substr(0, MAGIC.size()) != MAGIC) {
			error = "Not a packet bundle (DLUBNDL1)";
			return false;
		}
		uint32_t length{};
		std::memcpy(&length, data.data() + MAGIC.size(), sizeof(length));
		const size_t start = MAGIC.size() + 4;
		if (length > MAX_METADATA || data.size() - start < length) {
			error = "The bundle's metadata is cut short";
			return false;
		}
		bundle.meta = nlohmann::json::parse(data.substr(start, length), nullptr, false);
		if (bundle.meta.is_discarded() || !bundle.meta.is_object()) {
			error = "The bundle's metadata isn't JSON";
			return false;
		}
		bundle.records.clear();
		const bool whole = PacketRecord::ForEach(data.substr(start + length), [&](const PacketRecordHeader& header, std::string_view bytes) {
			bundle.records.push_back({ header, std::string(bytes) });
		});
		if (truncated) *truncated = !whole;
		return true;
	}

	inline bool Load(const std::filesystem::path& path, Bundle& bundle, std::string& error, bool* truncated = nullptr) {
		std::ifstream file(path, std::ios::binary);
		if (!file) {
			error = "Can't open " + path.string();
			return false;
		}
		std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return Parse(data, bundle, error, truncated);
	}

	inline bool Save(const std::filesystem::path& path, const Bundle& bundle) {
		std::string out = Header(bundle.meta);
		for (const auto& record : bundle.records) AppendRecord(out, record);
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(out.data(), static_cast<std::streamsize>(out.size()));
		return static_cast<bool>(file);
	}
}

#endif  //!__CAPTUREBUNDLE__H__
