#include "ChatHistory.h"

#include <algorithm>

namespace ChatHistory {
	bool CanRead(const std::string& channel, const Access& access) {
		if (channel == "zone" || channel == "web") return true;
		if (channel == "team" || channel == "guild") return access.group;
		if (channel == "whisper") return access.whispers;
		return false;
	}

	nlohmann::json MessageJson(const IChatLog::ChatMessage& m, const Access& access) {
		const bool readable = CanRead(m.channel, access);
		return { {"id", m.id}, {"time", m.time}, {"channel", m.channel}, {"sender_id", std::to_string(m.senderId)}, {"sender_name", m.senderName},
			{"account_id", m.accountId}, {"recipient_id", std::to_string(m.recipientId)}, {"recipient_name", m.recipientName},
			{"zone_id", m.zoneId}, {"instance_id", m.instanceId}, {"clone_id", m.cloneId}, {"message", readable ? m.message : ""},
			{"blocked", m.blocked}, {"filtered", m.filtered}, {"guild_id", std::to_string(m.guildId)}, {"team_id", std::to_string(m.teamId)},
			{"redacted", !readable} };
	}

	IChatLog::ChatQuery ConversationQuery(const IChatLog::ChatMessage& m, const Access& access) {
		IChatLog::ChatQuery q;
		q.includePrivate = access.group;
		q.includeWhispers = access.whispers;
		q.channel = m.channel;
		if (m.channel == "zone") {
			q.zoneId = m.zoneId;
			q.instanceId = m.instanceId;
			// Zone 0 would mean any zone
			if (m.zoneId == 0) q.channel = "none";
		} else if (m.channel == "whisper") {
			q.characterId = m.senderId;
			q.otherCharacterId = m.recipientId;
			if (m.senderId == 0 || m.recipientId == 0) q.channel = "none";
		} else if (m.channel == "team") {
			q.teamId = m.teamId;
			if (m.teamId == 0) q.channel = "none"; // logged before teams were recorded: no way to tell them apart
		} else if (m.channel == "guild") {
			q.guildId = m.guildId;
			if (m.guildId == 0) q.channel = "none";
		}
		return q;
	}

	bool SameConversation(const IChatLog::ChatMessage& a, const IChatLog::ChatMessage& b) {
		if (a.channel != b.channel) return false;
		if (a.channel == "zone") return a.zoneId == b.zoneId && a.instanceId == b.instanceId;
		if (a.channel == "whisper") {
			return (a.senderId == b.senderId && a.recipientId == b.recipientId) || (a.senderId == b.recipientId && a.recipientId == b.senderId);
		}
		if (a.channel == "team") return a.teamId == b.teamId;
		if (a.channel == "guild") return a.guildId == b.guildId;
		return true; // web
	}

	std::optional<std::string> ParseStatus(const std::string& status) {
		if (status == "open" || status == "actioned" || status == "dismissed") return status;
		return std::nullopt;
	}

	std::string StatusAction(const std::string& from, const std::string& to) {
		if (from == to) return "";
		if (to == "open") return "reopened";
		return to;
	}

	std::string Excerpt(const std::vector<IChatLog::ChatMessage>& messages, const size_t maxBytes) {
		std::string text;
		for (const auto& m : messages) {
			if (!text.empty()) text += " / ";
			text += m.senderName + ": " + m.message;
		}
		if (text.size() <= maxBytes) return text;
		const std::string ellipsis = "\xE2\x80\xA6"; // …
		size_t cut = maxBytes >= ellipsis.size() ? maxBytes - ellipsis.size() : 0;
		// Back up to the start of a UTF-8 character
		while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) cut--;
		return text.substr(0, cut) + ellipsis;
	}

	nlohmann::json Snapshot(const std::vector<IChatLog::ChatMessage>& before, const std::vector<IChatLog::ChatMessage>& flagged,
		const std::vector<IChatLog::ChatMessage>& after) {
		const Access all{ true, true };
		nlohmann::json out = nlohmann::json::array();
		const auto add = [&](const std::vector<IChatLog::ChatMessage>& list, bool isFlagged) {
			for (const auto& m : list) {
				auto json = MessageJson(m, all);
				json.erase("redacted");
				json["flagged"] = isFlagged;
				out.push_back(std::move(json));
			}
		};
		add(before, false);
		add(flagged, true);
		add(after, false);
		std::stable_sort(out.begin(), out.end(), [](const nlohmann::json& a, const nlohmann::json& b) { return a.value("id", uint64_t{}) < b.value("id", uint64_t{}); });
		return out;
	}

	nlohmann::json RedactSnapshot(const nlohmann::json& snapshot, const Access& access) {
		nlohmann::json out = nlohmann::json::array();
		if (!snapshot.is_array()) return out;
		for (auto message : snapshot) {
			if (!message.is_object()) continue;
			const bool readable = CanRead(message.value("channel", ""), access);
			if (!readable) message["message"] = "";
			message["redacted"] = !readable;
			out.push_back(std::move(message));
		}
		return out;
	}

	std::optional<IChatLog::ChatMessage> Subject(const std::vector<IChatLog::ChatMessage>& flagged, const LWOOBJID chosen) {
		if (chosen != 0) {
			for (const auto& m : flagged) if (m.senderId == chosen) return m;
		}
		for (const auto& m : flagged) if (m.senderId != 0) return m;
		return std::nullopt;
	}
}
