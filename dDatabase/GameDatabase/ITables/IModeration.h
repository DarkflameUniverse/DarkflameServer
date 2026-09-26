#ifndef __IMODERATION__H__
#define __IMODERATION__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Moderation tools: the automatic steps strikes set off, reports players send from the game, the chat filter's words
 * added by staff, and the addresses accounts log in from (for finding accounts that belong together).
 */
class IModeration {
public:
	// ---- Strike steps ----

	struct AppliedStrikeStep {
		std::string step;  // eStrikeStep's name
		uint32_t count{};  // the number of active strikes it was applied for
		int64_t time{};
	};

	// Note on a strike which step it set off, and for how many strikes
	virtual void SetStrikeStep(uint64_t strikeId, const std::string& step, uint32_t count) = 0;
	// Steps set off by the account's strikes given at or after `since` (revoked strikes included: the step still happened)
	virtual std::vector<AppliedStrikeStep> GetAppliedStrikeSteps(uint32_t accountId, int64_t since) = 0;

	// ---- Player reports ----

	struct PlayerReport {
		uint64_t id{};
		int64_t createdAt{};
		std::string kind;            // ePlayerReportKind's name
		LWOOBJID reporterId{};       // the reporting character
		uint32_t reporterAccountId{};
		LWOOBJID objectId{};         // what they picked in the game
		int32_t objectLot{};
		LWOOBJID targetCharacterId{}; // the player it is about: the reported player, or the model's or property's owner (0: unknown)
		uint32_t targetAccountId{};
		LWOOBJID propertyId{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		std::string body;
		uint8_t status{};            // ePlayerReportStatus
		std::string handledBy;
		int64_t handledAt{};
		std::string resolution;
	};

	struct PlayerReportQuery {
		int16_t status{ -1 };        // ePlayerReportStatus, -1: any
		uint32_t accountId{};        // reported account (0: any)
		uint32_t limit{ 100 };
		uint32_t offset{};
	};

	virtual uint64_t InsertPlayerReport(const PlayerReport& report) = 0;
	virtual std::vector<PlayerReport> GetPlayerReports(const PlayerReportQuery& query) = 0; // newest first
	virtual uint32_t CountPlayerReports(const PlayerReportQuery& query) = 0;
	virtual std::optional<PlayerReport> GetPlayerReport(uint64_t id) = 0;
	virtual void SetPlayerReportStatus(uint64_t id, uint8_t status, const std::string& handledBy, const std::string& resolution, int64_t time) = 0;

	// ---- Chat filter words ----

	struct ChatFilterWord {
		std::string word;            // lower case
		bool allowed{};              // true: accepted in whitelisted chat; false: always stopped
		std::string addedBy;
		int64_t addedAt{};
	};

	virtual std::vector<ChatFilterWord> GetChatFilterWords() = 0;
	// Adds the word, or moves it to the other list
	virtual void SetChatFilterWord(const ChatFilterWord& word) = 0;
	virtual bool DeleteChatFilterWord(const std::string& word) = 0;

	// ---- Login addresses and linked accounts ----

	// A successful game login from `address` (no port)
	virtual void RecordLoginAddress(uint32_t accountId, const std::string& address, int64_t time) = 0;

	struct LinkedAccount {
		uint32_t accountId{};
		std::string name;
		uint8_t gmLevel{};
		bool banned{};
		uint8_t link{};              // eAccountLink
		uint32_t shared{};           // LOGIN_ADDRESS: how many addresses they share; PLAY_KEY: accounts on the key
		int64_t lastSeen{};          // LOGIN_ADDRESS: the other account's latest login from a shared address
	};

	// Other accounts with the same play key, email address or login address as this one
	virtual std::vector<LinkedAccount> GetLinkedAccounts(uint32_t accountId) = 0;
	// How many login addresses are recorded for the account
	virtual uint32_t CountLoginAddresses(uint32_t accountId) = 0;
};

#endif  //!__IMODERATION__H__
