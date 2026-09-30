#include "CaptureTools.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "PacketDecoder.h"
#include "ServiceType.h"

namespace {
	using json = nlohmann::json;
	using Record = CaptureBundle::Record;

	const char* SourceName(uint8_t source) {
		switch (static_cast<eCaptureSource>(source)) {
		case eCaptureSource::AUTH: return "auth";
		case eCaptureSource::CHAT: return "chat";
		case eCaptureSource::WORLD: return "world";
		case eCaptureSource::MASTER: return "master";
		default: return "?";
		}
	}

	// Who is on each end, as the viewer shows it
	std::pair<std::string, std::string> Ends(const PacketRecordHeader& h) {
		std::string server = SourceName(h.source);
		if (h.source == static_cast<uint8_t>(eCaptureSource::WORLD) && h.zoneId) server += " " + std::to_string(h.zoneId) + ":" + std::to_string(h.instanceId);
		std::string other;
		if (h.flags & PacketRecordFlags::MASTER_LINK) other = "master";
		else if (h.source == static_cast<uint8_t>(eCaptureSource::CHAT) || h.source == static_cast<uint8_t>(eCaptureSource::MASTER)) other = "server";
		else other = "client";
		if (h.flags & PacketRecordFlags::BROADCAST) other = "everyone";
		return h.direction == static_cast<uint8_t>(ePacketDirection::RECEIVED) ? std::pair{ other, server } : std::pair{ server, other };
	}

	void ReplaceAll(std::string& bytes, int64_t from, int64_t to) {
		if (from == 0 || from == to) return;
		char a[8], b[8];
		std::memcpy(a, &from, 8);
		std::memcpy(b, &to, 8);
		const std::string_view needle(a, 8);
		for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 8)) std::memcpy(bytes.data() + at, b, 8);
	}

	// Remove volatile fields from decoded JSON, recursively
	void Strip(json& value) {
		if (value.is_object()) {
			for (auto it = value.begin(); it != value.end();) {
				if (CaptureTools::IsVolatileField(it.key())) it = value.erase(it);
				else {
					Strip(it.value());
					++it;
				}
			}
		} else if (value.is_array()) {
			for (auto& item : value) Strip(item);
		}
	}

	std::string NameOf(const Record& record) {
		return PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header)).name;
	}

	// How answers pair up in a diff: by name, and constructions by what they construct
	std::string PairKey(const Record& record) {
		const auto decoded = PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header));
		if (decoded.name == "ID_REPLICA_MANAGER_CONSTRUCTION" && decoded.fields) return decoded.name + " LOT " + std::to_string((*decoded.fields)["lot"].get<int32_t>());
		return decoded.name;
	}
}

namespace CaptureTools {
	void SortTimeline(std::vector<Record>& records) {
		std::stable_sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
			if (a.header.timeUs != b.header.timeUs) return a.header.timeUs < b.header.timeUs;
			if (a.header.source != b.header.source) return a.header.source < b.header.source;
			return a.header.seq < b.header.seq;
		});
	}

	bool FromClient(const PacketRecordHeader& h) {
		const auto source = static_cast<eCaptureSource>(h.source);
		return h.direction == static_cast<uint8_t>(ePacketDirection::RECEIVED) && !(h.flags & PacketRecordFlags::MASTER_LINK) &&
			(source == eCaptureSource::AUTH || source == eCaptureSource::WORLD);
	}

	bool IsVolatileField(const std::string& name) {
		static const std::set<std::string> fields{
			// Made by the server each run
			"objectID", "objectId", "lootID", "lootOwnerID", "requestID", "i64LocalID", "uiSkillHandle", "uiBehaviorHandle",
			// Time and where things run
			"timestamp", "stamps", "instanceID", "instanceId", "zoneInstance", "cloneID", "zoneClone", "serverIP", "serverPort",
			"worldServerIP", "worldServerPort", "processID", "port",
			// Per account on each server
			"playerID", "targetID", "senderID", "username", "networkID",
		};
		return fields.contains(name);
	}

	uint64_t ReplicaConnection(const PacketRecordHeader& h) {
		return (static_cast<uint64_t>(h.zoneId) << 48) ^ (static_cast<uint64_t>(h.instanceId) << 32) ^ h.cloneId;
	}

	json RecordJson(const Record& record, size_t index, int64_t startUs, bool fields, const json* replicaFields) {
		const auto& h = record.header;
		json out{
			{"i", index},
			{"t", static_cast<double>(h.timeUs - startUs) / 1000.0},
			{"time", h.timeUs / 1000},
			{"source", SourceName(h.source)},
			{"seq", h.seq},
			{"zone", h.zoneId}, {"instance", h.instanceId}, {"clone", h.cloneId},
			{"account", h.accountId},
			{"character", std::to_string(h.characterId)},
			{"peer", PacketRecord::PeerText(h.peer)},
			{"bits", h.bits},
			{"bytes", record.bytes.size()},
			{"cut", (h.flags & PacketRecordFlags::CUT) != 0},
		};
		if (h.flags & PacketRecordFlags::GAP) {
			out["gap"] = h.bits;
			out["name"] = "(" + std::to_string(h.bits) + " packets lost)";
			return out;
		}
		const auto [from, to] = Ends(h);
		out["from"] = from;
		out["to"] = to;
		out["toServer"] = FromClient(h);
		const auto decoded = PacketDecoder::Decode(record.bytes, FromClient(h));
		out["service"] = decoded.service;
		out["name"] = decoded.name;
		if (decoded.gameMessageId >= 0) {
			out["gameMessage"] = decoded.gameMessageId;
			out["object"] = std::to_string(decoded.objectId);
		}
		if (decoded.failed) out["unreadable"] = true;
		if (fields && replicaFields) out["fields"] = *replicaFields;
		else if (fields && decoded.fields) out["fields"] = *decoded.fields;
		return out;
	}

	std::vector<Track> Tracks(const std::vector<Record>& records, int64_t startUs) {
		std::vector<Track> tracks;
		for (const auto& record : records) {
			if (!FromClient(record.header) || record.header.source != static_cast<uint8_t>(eCaptureSource::WORLD)) continue;
			const auto position = PacketDecoder::Position(record.bytes);
			if (!position) continue;
			const auto& h = record.header;
			auto it = std::ranges::find_if(tracks, [&](const Track& t) { return t.characterId == h.characterId && t.zoneId == h.zoneId && t.instanceId == h.instanceId; });
			if (it == tracks.end()) {
				tracks.push_back({ h.characterId, h.zoneId, h.instanceId, {} });
				it = tracks.end() - 1;
			}
			it->samples.insert(it->samples.end(), { static_cast<float>(h.timeUs - startUs) / 1e6f, position->x, position->y, position->z });
		}
		return tracks;
	}

	std::vector<WorldVisit> Worlds(const std::vector<Record>& records, int64_t startUs) {
		std::vector<WorldVisit> visits;
		std::map<LWOOBJID, size_t> last; // character -> their latest entry in visits
		for (const auto& record : records) {
			const auto& h = record.header;
			// Only what the client sent: the old world can still send a few packets after the client reached the new one
			if (h.source != static_cast<uint8_t>(eCaptureSource::WORLD) || !h.characterId || (h.flags & PacketRecordFlags::GAP) || !FromClient(h)) continue;
			const auto it = last.find(h.characterId);
			if (it != last.end() && visits[it->second].zoneId == h.zoneId && visits[it->second].instanceId == h.instanceId) continue;
			last[h.characterId] = visits.size();
			visits.push_back({ h.characterId, static_cast<float>(h.timeUs - startUs) / 1e6f, h.zoneId, h.instanceId, h.cloneId });
		}
		return visits;
	}

	std::map<std::string, LWOOBJID> MakePortable(CaptureBundle::Bundle& bundle) {
		std::map<LWOOBJID, std::string> characters;
		std::map<uint32_t, uint32_t> accounts;
		for (const auto& record : bundle.records) {
			if (record.header.characterId && !characters.contains(record.header.characterId)) {
				characters[record.header.characterId] = "char#" + std::to_string(characters.size() + 1);
			}
			if (record.header.accountId && !accounts.contains(record.header.accountId)) accounts[record.header.accountId] = static_cast<uint32_t>(accounts.size() + 1);
		}
		json ids = json::object();
		std::map<std::string, LWOOBJID> found;
		std::map<LWOOBJID, LWOOBJID> placeholders;
		size_t n = 0;
		for (const auto& [id, symbol] : characters) {
			const auto placeholder = PLACEHOLDER_BASE + static_cast<int64_t>(++n);
			placeholders[id] = placeholder;
			ids[symbol] = { {"kind", "character"}, {"placeholder", std::to_string(placeholder)} };
			found[symbol] = id;
		}
		for (const auto& [id, number] : accounts) ids["account#" + std::to_string(number)] = { {"kind", "account"}, {"placeholder", number} };
		for (auto& record : bundle.records) {
			for (const auto& [id, placeholder] : placeholders) ReplaceAll(record.bytes, id, placeholder);
			if (record.header.characterId) record.header.characterId = placeholders[record.header.characterId];
			if (record.header.accountId) record.header.accountId = accounts[record.header.accountId];
			record.header.peer = 0;
			PacketDecoder::Scrub(record.bytes, false);
			record.header.length = static_cast<uint32_t>(record.bytes.size());
		}
		bundle.meta["ids"] = ids;
		bundle.meta["portable"] = true;
		return found;
	}

	size_t Anonymise(CaptureBundle::Bundle& bundle) {
		size_t changed = 0;
		for (auto& record : bundle.records) {
			if (PacketDecoder::Scrub(record.bytes, true)) changed++;
			record.header.length = static_cast<uint32_t>(record.bytes.size());
			record.header.peer = 0;
		}
		bundle.meta["anonymised"] = true;
		return changed;
	}

	json DiffReport::ToJson() const {
		return { {"expected", expected}, {"matched", matched}, {"differing", differing}, {"missing", missing}, {"extra", extra},
			{"differingByName", differingByName}, {"missingByName", missingByName}, {"extraByName", extraByName}, {"examples", examples} };
	}

	DiffReport Diff(const std::vector<Record>& expected, const std::vector<Record>& actual) {
		DiffReport report;
		// Server -> client packets only (what the server answered)
		const auto answers = [](const std::vector<Record>& records) {
			std::vector<const Record*> out;
			for (const auto& r : records) {
				if (r.header.flags & (PacketRecordFlags::GAP | PacketRecordFlags::MASTER_LINK)) continue;
				const auto source = static_cast<eCaptureSource>(r.header.source);
				if ((source == eCaptureSource::AUTH || source == eCaptureSource::WORLD) && r.header.direction == static_cast<uint8_t>(ePacketDirection::SENT)) out.push_back(&r);
			}
			return out;
		};
		const auto want = answers(expected), got = answers(actual);
		report.expected = want.size();
		std::vector<std::string> gotNames;
		gotNames.reserve(got.size());
		for (const auto* g : got) gotNames.push_back(PairKey(*g));
		// Each recorded answer pairs with the next unpaired replayed answer of the same name (order kept per name)
		std::map<std::string, std::vector<size_t>> byName;
		for (size_t i = 0; i < got.size(); i++) byName[gotNames[i]].push_back(i);
		std::map<std::string, size_t> next;
		std::vector<bool> used(got.size());
		for (const auto* w : want) {
			const auto name = PairKey(*w);
			const auto& candidates = byName[name];
			auto& at = next[name];
			const size_t found = at < candidates.size() ? candidates[at++] : got.size();
			if (found == got.size()) {
				report.missing++;
				report.missingByName[name]++;
				continue;
			}
			used[found] = true;
			auto a = PacketDecoder::Decode(w->bytes, false).fields.value_or(json());
			auto b = PacketDecoder::Decode(got[found]->bytes, false).fields.value_or(json());
			Strip(a);
			Strip(b);
			// Packets without decoded fields compare by size
			const bool same = a.is_null() && b.is_null() ? w->bytes.size() == got[found]->bytes.size() : a == b;
			if (same) {
				report.matched++;
			} else {
				report.differing++;
				report.differingByName[name]++;
				if (report.examples.size() < 20) {
					report.examples.push_back(name + ": recorded " + (a.is_null() ? std::to_string(w->bytes.size()) + " bytes" : a.dump()).substr(0, 300) +
						" / replayed " + (b.is_null() ? std::to_string(got[found]->bytes.size()) + " bytes" : b.dump()).substr(0, 300));
				}
			}
		}
		for (size_t i = 0; i < got.size(); i++) {
			if (used[i]) continue;
			report.extra++;
			report.extraByName[gotNames[i]]++;
		}
		return report;
	}
}
