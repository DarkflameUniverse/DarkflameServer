#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GeneralUtils.h"
#include "json.hpp"
#include "magic_enum.hpp"
#include "IMessageCaptures.h"
#include "master/MessageCapture.h"
#include "MessageType/Game.h"

/**
 * How the message inspector shows captured game messages, reads the message filters staff type in, stores captures
 * and picks old ones to delete. Message names come from the server's MessageType::Game enum. Pure, so it is unit tested.
 */
namespace InspectorFormat {
	// "REQUEST_USE", or "#1234" for an ID the server has no name for
	inline std::string MessageName(uint16_t id) {
		const auto name = magic_enum::enum_name(static_cast<MessageType::Game>(id));
		return name.empty() ? "#" + std::to_string(id) : std::string(name);
	}

	// Every message the server knows: [{id, name}]
	inline nlohmann::json AllMessages() {
		nlohmann::json list = nlohmann::json::array();
		for (const auto& [value, name] : magic_enum::enum_entries<MessageType::Game>()) {
			list.push_back({ {"id", static_cast<uint16_t>(value)}, {"name", name} });
		}
		return list;
	}

	/**
	 * A message typed in a filter: its name (any case, e.g. "request_use") or its number. nullopt if it is neither.
	 */
	inline std::optional<uint16_t> ParseMessage(const nlohmann::json& value) {
		if (value.is_number_unsigned() && value.get<uint64_t>() <= UINT16_MAX) return static_cast<uint16_t>(value.get<uint64_t>());
		if (!value.is_string()) return std::nullopt;
		std::string text = value.get<std::string>();
		text.erase(0, text.find_first_not_of(" \t"));
		text.erase(text.find_last_not_of(" \t") + 1);
		if (text.empty()) return std::nullopt;
		if (std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) {
			const auto number = std::stoul(text.substr(0, 6));
			return number <= UINT16_MAX ? std::optional<uint16_t>(static_cast<uint16_t>(number)) : std::nullopt;
		}
		for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		const auto parsed = magic_enum::enum_cast<MessageType::Game>(text);
		return parsed ? std::optional<uint16_t>(static_cast<uint16_t>(*parsed)) : std::nullopt;
	}

	/**
	 * A filter list (array of names or numbers). Sets `error` to the first entry it can't read and returns nullopt.
	 */
	inline std::optional<std::vector<uint16_t>> ParseMessageList(const nlohmann::json& list, std::string& error) {
		std::vector<uint16_t> ids;
		if (list.is_null()) return ids;
		if (!list.is_array() || list.size() > MessageCapture::MAX_FILTER) {
			error = "A message list must be an array of at most " + std::to_string(MessageCapture::MAX_FILTER) + " names or numbers";
			return std::nullopt;
		}
		for (const auto& item : list) {
			const auto id = ParseMessage(item);
			if (!id) {
				error = "Unknown game message: " + (item.is_string() ? item.get<std::string>() : item.dump());
				return std::nullopt;
			}
			if (std::find(ids.begin(), ids.end(), *id) == ids.end()) ids.push_back(*id);
		}
		return ids;
	}

	inline nlohmann::json MessageNames(const std::vector<uint16_t>& ids) {
		nlohmann::json names = nlohmann::json::array();
		for (const auto id : ids) names.push_back(MessageName(id));
		return names;
	}

	// Message IDs as stored with a session: "154,1234"
	inline std::string IdsText(const std::vector<uint16_t>& ids) {
		std::string text;
		for (const auto id : ids) text += (text.empty() ? "" : ",") + std::to_string(id);
		return text;
	}

	// Stored message IDs back as a list; anything that isn't a number is skipped
	inline std::vector<uint16_t> ParseIds(const std::string& text) {
		std::vector<uint16_t> ids;
		size_t start = 0;
		while (start <= text.size()) {
			const auto end = std::min(text.find(',', start), text.size());
			const auto number = GeneralUtils::TryParse<uint16_t>(text.substr(start, end - start));
			if (number) ids.push_back(*number);
			start = end + 1;
		}
		return ids;
	}

	/**
	 * Messages the world left out before each one it sends. A world numbers what it captures from 1 and counts the
	 * ones it drops too, so a jump in its numbers is how many it left out. After the capture (re)starts in a world the
	 * numbers start over, so the first message there never counts as a jump.
	 */
	class GapCounter {
	public:
		// The capture (re)started in a world
		void Restart() { m_Last = 0; }

		// Messages left out just before the one the world numbered `sequence`
		uint32_t Next(uint32_t sequence) {
			const uint32_t gap = m_Last != 0 && sequence > m_Last + 1 ? sequence - m_Last - 1 : 0;
			m_Last = sequence;
			return gap;
		}

	private:
		uint32_t m_Last{};
	};

	// A captured message as it is saved with its session
	inline IMessageCaptures::MessageCaptureRecord ToRecord(const MessageCaptureEntry& entry, uint64_t sessionId, uint32_t seq, uint32_t droppedBefore,
		uint32_t zoneId, uint32_t instanceId, uint32_t cloneId) {
		return { sessionId, seq, entry.timeMs, static_cast<uint8_t>(entry.direction), entry.messageId, entry.objectId, entry.bits, droppedBefore,
			zoneId, instanceId, cloneId, entry.payload, entry.decoded };
	}

	// About what a saved message takes in the database: its bytes and fields plus the row's other columns
	inline uint64_t StoredBytes(const IMessageCaptures::MessageCaptureRecord& record) {
		constexpr uint64_t ROW_OVERHEAD = 56;
		return ROW_OVERHEAD + record.payload.size() + record.decoded.size();
	}

	/**
	 * One captured message for the page: {seq, time (Unix ms), dir ("to_server"/"to_client"), id, name, object, bits,
	 * bytes, hex, truncated, fields, gap, zone, instance, clone}. `fields` is the decoded message (null when the server
	 * has no typed struct); `gap` is how many messages were left out just before it.
	 */
	inline nlohmann::json RecordJson(const IMessageCaptures::MessageCaptureRecord& record) {
		const auto bytes = (static_cast<uint64_t>(record.bits) + 7) / 8;
		nlohmann::json fields = nullptr;
		if (!record.decoded.empty()) {
			fields = nlohmann::json::parse(record.decoded, nullptr, false);
			if (fields.is_discarded()) fields = nullptr;
		}
		return {
			{"seq", record.seq},
			{"time", record.timeMs},
			{"dir", record.direction == static_cast<uint8_t>(eMessageDirection::TO_SERVER) ? "to_server" : "to_client"},
			{"id", record.messageId},
			{"name", MessageName(record.messageId)},
			{"object", std::to_string(record.objectId)},
			{"bits", record.bits},
			{"bytes", bytes},
			{"hex", MessageCapture::ToHex(record.payload)},
			{"truncated", record.payload.size() < bytes},
			{"fields", fields},
			{"gap", record.droppedBefore},
			{"zone", record.zoneId},
			{"instance", record.instanceId},
			{"clone", record.cloneId}
		};
	}

	// A message straight from a world, numbered `sequence` (tests and the old single-message form)
	inline nlohmann::json EntryJson(const MessageCaptureEntry& entry) {
		return RecordJson(ToRecord(entry, 0, entry.sequence, 0, 0, 0, 0));
	}

	// "started" -> STARTED; nullopt for anything else
	inline std::optional<IMessageCaptures::eSessionOrder> ParseOrder(std::string text) {
		for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		return magic_enum::enum_cast<IMessageCaptures::eSessionOrder>(text);
	}

	// The orders ParseOrder reads: ["started", "character", ...]
	inline std::vector<std::string> OrderNames() {
		std::vector<std::string> names;
		for (const auto name : magic_enum::enum_names<IMessageCaptures::eSessionOrder>()) {
			std::string lower(name);
			for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			names.push_back(std::move(lower));
		}
		return names;
	}

	// What the retention rule looks at for one saved session
	struct StoredSession {
		uint64_t id{};
		int64_t startedAt{};
		uint64_t bytes{};
		bool running{};
	};

	/**
	 * The saved sessions to delete: every finished one that started more than `days` ago (0: no age limit), then the
	 * oldest finished ones until what is left fits in `maxBytes` (0: no size limit). Running captures are never deleted
	 * but count toward the size.
	 */
	inline std::vector<uint64_t> SelectExpired(std::vector<StoredSession> sessions, int64_t now, int64_t days, uint64_t maxBytes) {
		std::ranges::sort(sessions, [](const StoredSession& a, const StoredSession& b) { return a.startedAt != b.startedAt ? a.startedAt < b.startedAt : a.id < b.id; });
		std::vector<uint64_t> expired;
		uint64_t kept = 0;
		std::vector<const StoredSession*> left;
		for (const auto& session : sessions) {
			if (!session.running && days > 0 && session.startedAt < now - days * 24 * 60 * 60) {
				expired.push_back(session.id);
				continue;
			}
			kept += session.bytes;
			left.push_back(&session);
		}
		if (maxBytes == 0) return expired;
		for (const auto* session : left) {
			if (kept <= maxBytes) break;
			if (session->running) continue;
			expired.push_back(session->id);
			kept -= session->bytes;
		}
		return expired;
	}
}
