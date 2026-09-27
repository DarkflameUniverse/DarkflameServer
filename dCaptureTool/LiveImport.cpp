#include "LiveImport.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <regex>
#include <set>

#include "ClientPackets.h"
#include "CaptureTools.h"
#include "MessageIdentifiers.h"
#include "MessageType/Client.h"
#include "PacketDecoder.h"
#include "ServiceType.h"
#include "ZCompression.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
	constexpr uint16_t AUTH_PORT = 1001;

	struct Entry {
		std::string name;
		std::string data;
		int64_t timeUs{};
	};

	template<typename T>
	T Get(const std::string& data, size_t at) {
		T value{};
		if (at + sizeof(T) <= data.size()) std::memcpy(&value, data.data() + at, sizeof(T));
		return value;
	}

	int64_t DosTimeUs(uint16_t time, uint16_t date) {
		std::tm tm{};
		tm.tm_year = ((date >> 9) & 0x7f) + 80;
		tm.tm_mon = ((date >> 5) & 0x0f) - 1;
		tm.tm_mday = date & 0x1f;
		tm.tm_hour = (time >> 11) & 0x1f;
		tm.tm_min = (time >> 5) & 0x3f;
		tm.tm_sec = (time & 0x1f) * 2;
		return static_cast<int64_t>(timegm(&tm)) * 1000000;
	}

	// The entries of a zip file (stored or deflated; no zip64, no encryption)
	bool ReadZip(const fs::path& path, std::vector<Entry>& entries, std::string& error) {
		std::ifstream file(path, std::ios::binary);
		const std::string zip((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (zip.size() < 22) {
			error = "not a zip file";
			return false;
		}
		size_t end = std::string::npos;
		for (size_t at = zip.size() - 22 + 1; at-- > 0 && zip.size() - at < 65557;) {
			if (Get<uint32_t>(zip, at) == 0x06054b50) {
				end = at;
				break;
			}
		}
		if (end == std::string::npos) {
			error = "no zip directory";
			return false;
		}
		const auto count = Get<uint16_t>(zip, end + 10);
		size_t at = Get<uint32_t>(zip, end + 16);
		for (uint16_t i = 0; i < count; i++) {
			if (Get<uint32_t>(zip, at) != 0x02014b50) {
				error = "damaged zip directory";
				return false;
			}
			const auto flags = Get<uint16_t>(zip, at + 8);
			const auto method = Get<uint16_t>(zip, at + 10);
			const auto time = Get<uint16_t>(zip, at + 12), date = Get<uint16_t>(zip, at + 14);
			const auto compressed = Get<uint32_t>(zip, at + 20), size = Get<uint32_t>(zip, at + 24);
			const auto nameLength = Get<uint16_t>(zip, at + 28), extraLength = Get<uint16_t>(zip, at + 30), commentLength = Get<uint16_t>(zip, at + 32);
			const auto local = Get<uint32_t>(zip, at + 42);
			Entry entry;
			entry.name = zip.substr(at + 46, nameLength);
			entry.timeUs = DosTimeUs(time, date);
			at += 46 + nameLength + extraLength + commentLength;
			if (flags & 1) continue; // encrypted: left alone
			if (Get<uint32_t>(zip, local) != 0x04034b50) continue;
			const size_t data = local + 30 + Get<uint16_t>(zip, local + 26) + Get<uint16_t>(zip, local + 28);
			if (data + compressed > zip.size()) continue;
			const std::string_view raw(zip.data() + data, compressed);
			if (method == 0) entry.data = std::string(raw);
			else if (method == 8) {
				auto inflated = ZCompression::InflateRaw(raw, size);
				if (!inflated) continue;
				entry.data = std::move(*inflated);
			} else continue;
			entries.push_back(std::move(entry));
		}
		return true;
	}

	/**
	 * The live servers' CREATE_CHARACTER: a compressed LDF list with more types than DLU writes (so read here, not with
	 * ClientPackets::CreateCharacter): the object ID, name and character XML.
	 */
	bool ReadCreateCharacter(const std::string& bytes, LWOOBJID& id, std::string& name, std::string& xml) {
		if (bytes.size() < 8 + 13) return false;
		const auto compressedSize = Get<uint32_t>(bytes, 8 + 9), size = Get<uint32_t>(bytes, 8 + 5);
		if (bytes[8 + 4] != 1 || 8 + 13 + static_cast<size_t>(compressedSize) > bytes.size() || size > 64 * 1024 * 1024) return false;
		std::vector<uint8_t> out(size);
		int32_t error{};
		if (ZCompression::Decompress(reinterpret_cast<const uint8_t*>(bytes.data()) + 21, compressedSize, out.data(), size, error) != static_cast<int32_t>(size)) return false;
		const std::string data(out.begin(), out.end());
		size_t at = 4;
		for (uint32_t i = 0, count = Get<uint32_t>(data, 0); i < count && at < data.size(); i++) {
			const uint8_t keyBytes = static_cast<uint8_t>(data[at++]);
			std::u16string key(keyBytes / 2, u'\0');
			std::memcpy(key.data(), data.data() + at, keyBytes);
			at += keyBytes;
			const uint8_t type = static_cast<uint8_t>(data[at++]);
			switch (type) {
			case 0: { // UTF-16
				const auto length = Get<uint32_t>(data, at);
				std::u16string value(length, u'\0');
				std::memcpy(value.data(), data.data() + at + 4, std::min<size_t>(length * 2, data.size() - at - 4));
				if (key == u"name") name = GeneralUtils::UTF16ToWTF8(value);
				at += 4 + length * 2;
				break;
			}
			case 13: { // UTF-8
				const auto length = Get<uint32_t>(data, at);
				if (key == u"xmlData") xml = data.substr(at + 4, length);
				at += 4 + length;
				break;
			}
			case 1: case 3: case 5: at += 4; break;
			case 7: at += 1; break;
			case 4: case 8: at += 8; break;
			case 9:
				if (key == u"objid") id = Get<int64_t>(data, at);
				at += 8;
				break;
			default: return id != 0;
			}
		}
		return id != 0;
	}

	// auth_traffic, char_traffic, world_traffic, world1_traffic, world2_traffic, ...: the order they were played in
	int ZipOrder(const std::string& name) {
		if (name.starts_with("auth")) return 0;
		if (name.starts_with("char")) return 1;
		static const std::regex world(R"(world(\d*)_traffic)");
		std::smatch match;
		if (std::regex_search(name, match, world)) return 2 + (match[1].length() ? std::stoi(match[1]) : 0);
		return 1000;
	}
}

namespace LiveImport {
	std::vector<fs::path> FindScenarios(const fs::path& root) {
		std::set<fs::path> folders;
		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (ec) break;
			const auto name = it->path().filename().string();
			if (it->is_regular_file(ec) && name.ends_with("_traffic.zip")) folders.insert(it->path().parent_path());
		}
		return { folders.begin(), folders.end() };
	}

	Result Import(const fs::path& scenario) {
		Result result;
		std::vector<fs::path> zips;
		std::error_code ec;
		if (fs::is_directory(scenario, ec)) {
			for (const auto& entry : fs::directory_iterator(scenario, ec)) {
				const auto name = entry.path().filename().string();
				if (entry.is_regular_file() && name.ends_with("_traffic.zip")) zips.push_back(entry.path());
			}
		} else {
			zips.push_back(scenario);
		}
		std::ranges::sort(zips, [](const fs::path& a, const fs::path& b) { return ZipOrder(a.filename().string()) < ZipOrder(b.filename().string()); });
		if (zips.empty()) {
			result.error = "No *_traffic.zip files";
			return result;
		}

		// Split packets come as their parts ("(1of81)", left out) and joined ("<n>_<ports>_joined_[...]")
		static const std::regex packetName(R"(^(\d+)_(\d+)-(\d+)(?:_(\d+|joined))?_\[)");
		std::map<LWOOBJID, std::pair<std::string, std::string>> characters; // id -> name, xml
		int64_t last = 0;
		uint32_t seq = 0;
		for (const auto& zip : zips) {
			std::vector<Entry> entries;
			std::string error;
			if (!ReadZip(zip, entries, error)) {
				result.error = zip.filename().string() + ": " + error;
				return result;
			}
			result.zips++;
			struct Numbered { uint64_t index; uint32_t part; Entry* entry; uint16_t from; uint16_t to; };
			std::vector<Numbered> packets;
			for (auto& entry : entries) {
				std::smatch match;
				const auto name = fs::path(entry.name).filename().string();
				if (entry.data.empty() || !std::regex_search(name, match, packetName)) {
					result.skipped++;
					continue;
				}
				const auto part = match[4].str();
				packets.push_back({ std::stoull(match[1]), part.empty() || part == "joined" ? 1u : static_cast<uint32_t>(std::stoul(part)), &entry,
					static_cast<uint16_t>(std::stoul(match[2])), static_cast<uint16_t>(std::stoul(match[3])) });
			}
			std::ranges::sort(packets, [](const Numbered& a, const Numbered& b) { return a.index != b.index ? a.index < b.index : a.part < b.part; });

			const bool auth = zip.filename().string().starts_with("auth");
			uint16_t zone = 0, instance = 0;
			uint32_t clone = 0;
			LWOOBJID character = 0;
			for (const auto& p : packets) {
				// The server is the end with the lower port (auth 1001, worlds 2000 and up; clients' ports are higher)
				const uint16_t server = std::min(p.from, p.to), client = std::max(p.from, p.to);
				CaptureBundle::Record record;
				record.bytes = p.entry->data;
				if (!PacketDecoder::Redact(record.bytes)) {
					result.skipped++;
					continue;
				}
				auto& h = record.header;
				h.timeUs = last = std::max(last + 1000, p.entry->timeUs);
				h.seq = ++seq;
				h.source = static_cast<uint8_t>(auth || server == AUTH_PORT ? eCaptureSource::AUTH : eCaptureSource::WORLD);
				h.direction = static_cast<uint8_t>(p.to == server ? ePacketDirection::RECEIVED : ePacketDirection::SENT);
				h.peer = client;
				h.bits = static_cast<uint32_t>(record.bytes.size() * 8);
				h.length = static_cast<uint32_t>(record.bytes.size());

				// Where this is, and whose: from the zone the server loads and the character it makes
				const auto decoded = PacketDecoder::Decode(record.bytes, h.direction == static_cast<uint8_t>(ePacketDirection::RECEIVED));
				if (decoded.name == "LOAD_STATIC_ZONE" && decoded.fields) {
					zone = static_cast<uint16_t>((*decoded.fields)["mapID"].get<int>());
					instance = static_cast<uint16_t>((*decoded.fields)["instanceID"].get<int>());
					clone = (*decoded.fields)["cloneID"].get<uint32_t>();
				}
				if (decoded.name == "CREATE_CHARACTER") {
					LWOOBJID id{};
					std::string name, xml;
					if (ReadCreateCharacter(record.bytes, id, name, xml)) {
						character = id;
						characters[id] = { name, xml };
					}
				}
				h.zoneId = zone;
				h.instanceId = instance;
				h.cloneId = clone;
				h.characterId = character;
				result.bundle.records.push_back(std::move(record));
				result.packets++;
			}
		}

		auto& meta = result.bundle.meta;
		meta["format"] = CaptureBundle::FORMAT_VERSION;
		meta["origin"] = "live-2014";
		meta["scenario"] = scenario.filename().string();
		meta["server"] = { {"version", "LEGO Universe live servers"} };
		json zones = json::object();
		for (const auto& record : result.bundle.records) {
			const auto decoded = PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header));
			if (decoded.name == "LOAD_STATIC_ZONE" && decoded.fields) zones[std::to_string((*decoded.fields)["mapID"].get<int>())] = (*decoded.fields)["mapChecksum"];
			if (decoded.name == "VALIDATION" && decoded.fields) meta["fdbChecksum"] = (*decoded.fields)["fdbChecksum"];
		}
		meta["zones"] = zones;
		const auto symbols = CaptureTools::MakePortable(result.bundle);
		json setup = json::array();
		static const std::regex account(R"( acct="[0-9]+")");
		for (const auto& [symbol, id] : symbols) {
			const auto it = characters.find(id);
			if (it == characters.end()) continue;
			setup.push_back({ {"symbol", symbol}, {"placeholder", meta["ids"][symbol]["placeholder"]}, {"name", it->second.first},
				{"xml", std::regex_replace(it->second.second, account, "")} });
		}
		meta["setup"] = { {"characters", setup} };
		return result;
	}
}
