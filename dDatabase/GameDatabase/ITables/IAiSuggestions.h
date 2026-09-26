#ifndef __IAISUGGESTIONS__H__
#define __IAISUGGESTIONS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * The dashboard's staff-side AI moderator helper: every suggestion it asked for (who asked, about what, the model,
 * tokens used and the validated answer or the error), kept with the item so the same case isn't sent twice.
 * Suggestions are drafts for staff; nothing here is ever shown to players.
 */
class IAiSuggestions {
public:
	enum class eAiStatus : uint8_t {
		OK = 0,       // a validated suggestion
		REJECTED = 1, // the model answered, but the answer failed validation
		FAILED = 2    // no usable answer (API error, network, ...)
	};

	struct AiSuggestion {
		uint64_t id{};
		std::string kind;          // player_report, chat_message, player_chat, name, pet_name, economy_flag
		int64_t itemId{};          // the report, message, character, pet or flag
		std::string fingerprint;   // hash of what was sent (case, rules, model, prompt version)
		uint32_t requestedById{};
		std::string requestedBy;
		int64_t createdAt{};
		std::string model;
		uint32_t inputTokens{};
		uint32_t outputTokens{};
		eAiStatus status{ eAiStatus::OK };
		std::string suggestion;    // the validated answer as JSON, empty unless OK
		std::string error;
		std::string context;       // the case file that was sent, as JSON (for review)
	};

	virtual uint64_t InsertAiSuggestion(const AiSuggestion& suggestion) = 0;
	// Newest first
	virtual std::vector<AiSuggestion> GetAiSuggestions(const std::string& kind, int64_t itemId, uint32_t limit) = 0;
	// The newest OK suggestion made from exactly this input
	virtual std::optional<AiSuggestion> FindAiSuggestion(const std::string& kind, int64_t itemId, const std::string& fingerprint) = 0;

	struct AiUsage {
		uint32_t requests{};
		uint64_t inputTokens{};
		uint64_t outputTokens{};
	};
	// Requests (of any status) made at or after `since`, and their tokens
	virtual AiUsage GetAiUsageSince(int64_t since) = 0;

	// One economy flag's row, for sending it to the helper; nullopt when there is none
	struct EconomyFlagRow {
		uint64_t id{};
		int64_t createdAt{};
		uint32_t day{};
		uint8_t kind{};
		LWOOBJID characterId{};
		int32_t lot{};
		LWOOBJID itemId{};
		int64_t value{};
		int64_t baseline{};
		std::string details;
		uint8_t status{};
		std::string note;
	};
	virtual std::optional<EconomyFlagRow> GetEconomyFlagRow(uint64_t id) = 0;
};

#endif  //!__IAISUGGESTIONS__H__
