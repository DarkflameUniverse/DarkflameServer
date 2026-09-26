#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

/**
 * Prompts and answers for the AI moderator helper. Pure (no database, no config) so it can be unit tested.
 *
 * What players wrote (chat, report text, names) is untrusted: it only ever goes into the prompt as JSON string values
 * inside a <case_data_NONCE> block, with < > & escaped so nothing in it can close the block, and the model is told
 * the block is evidence, never instructions. The answer must be exactly one JSON object of a fixed shape; anything
 * else is rejected, as is an answer that repeats the hidden marker (canary) or the instructions.
 */
namespace ModeratorPrompt {
	// Bump when the prompt changes, so stored suggestions aren't reused for a different prompt
	constexpr const char* PROMPT_VERSION = "1";

	constexpr size_t MAX_PLAYER_REASON = 300;
	constexpr size_t MAX_STAFF_EXPLANATION = 2000;
	constexpr uint32_t MAX_DAYS = 365;

	enum class eKind : uint8_t { PLAYER_REPORT, CHAT_MESSAGE, PLAYER_CHAT, NAME, PET_NAME, ECONOMY_FLAG };

	std::optional<eKind> ParseKind(std::string_view key);
	std::string KindKey(eKind kind);   // player_report, chat_message, player_chat, name, pet_name, economy_flag
	std::string KindLabel(eKind kind); // "player report", ...

	// What the helper may suggest for each kind of item
	const std::vector<std::string>& Actions(eKind kind);

	struct ChatLine {
		int64_t time{};
		std::string channel;
		std::string from;
		std::string to;
		std::string text;
		bool blocked{};  // stopped by the chat filter
		bool subject{};  // the message being judged
	};

	struct StrikeLine {
		int64_t time{};
		std::string source;
		std::string subject;
		std::string reason;
		bool counts{};
	};

	// Earlier moderation of the account: notes, warnings, mutes, bans, earlier reports and name decisions
	struct HistoryLine {
		int64_t time{};
		std::string kind;
		std::string text;
	};

	struct Case {
		eKind kind{};
		nlohmann::json item = nlohmann::json::object(); // the item itself (may hold player-written strings)
		std::vector<ChatLine> chat;
		std::string chatNote;                           // why chat is missing or cut, if it is
		uint32_t activeStrikes{};
		std::vector<StrikeLine> strikes;
		std::vector<HistoryLine> history;
	};

	struct Prompt {
		std::string system;
		std::string user;
		nlohmann::json schema;
		std::set<std::string> refs; // M1, S1, H1, ITEM: what the answer may cite
		std::string nonce;
		std::string canary;
		// Long lines of the instructions (never to be repeated anywhere) and of the rules (never in player_reason)
		std::vector<std::string> instructionLines;
		std::vector<std::string> rulesLines;
	};

	// Cut text to at most `bytes` bytes without splitting a UTF-8 character
	std::string Clip(std::string_view text, size_t bytes);

	// UTC time as "2026-09-26 14:03"
	std::string FormatTime(int64_t unixTime);

	// JSON with < > & written as \u escapes (and invalid UTF-8 replaced), so player text can't form tags
	std::string SafeJson(const nlohmann::json& value);

	// The case file sent to the model; fills `refs` with the labels it gives chat lines, strikes and history
	nlohmann::json CaseJson(const Case& c, std::set<std::string>* refs = nullptr);

	// The JSON schema of an answer for this kind
	nlohmann::json Schema(eKind kind);

	// Random hex token (for nonces and canaries)
	std::string RandomToken(size_t bytes = 12);

	Prompt Build(const Case& c, const std::string& rules, const std::string& nonce, const std::string& canary);

	// Same case, rules and model: the same fingerprint, so a stored suggestion can be reused instead of asking again
	std::string Fingerprint(const Case& c, const std::string& rules, const std::string& model);

	struct Suggestion {
		std::string action;
		uint32_t days{};
		bool strike{};
		std::string playerReason;
		std::string staffExplanation;
		std::string confidence;
		std::vector<std::string> evidence;

		nlohmann::json ToJson() const;
	};

	struct Parsed {
		std::optional<Suggestion> suggestion;
		std::string error; // why the answer was rejected
	};

	// Strictly check the model's answer: exactly one JSON object with exactly the expected fields and values
	Parsed Parse(const std::string& text, eKind kind, const Prompt& prompt);
}
