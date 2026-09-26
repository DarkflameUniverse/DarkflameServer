#ifndef __IRELATEDDATA__H__
#define __IRELATEDDATA__H__

#include <cstdint>
#include <string>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Everything tied to one account or character, for the "related" sections of the dashboard's account and character
 * pages. Each returns a JSON array of rows, newest first where that means something.
 */
class IRelatedData {
public:
	virtual nlohmann::json GetPropertiesOwnedBy(LWOOBJID characterId) = 0;
	virtual nlohmann::json GetBugReportsBy(LWOOBJID characterId, uint32_t limit) = 0;
	virtual nlohmann::json GetEconomyFlagsFor(LWOOBJID characterId, uint32_t limit) = 0;
	virtual nlohmann::json GetFriendsOf(LWOOBJID characterId) = 0;
	virtual nlohmann::json GetCheatDetectionsFor(uint32_t accountId, uint32_t limit) = 0;
	// Dashboard actions about the account or one of its characters
	virtual nlohmann::json GetAuditAbout(uint32_t accountId, uint32_t limit) = 0;
	// Every column of one property, 64-bit ids as strings; empty object when missing
	virtual nlohmann::json GetPropertyRecord(LWOOBJID propertyId) = 0;
	// Every properties_contents column of the property's models, with the ugc blueprint (without the LXFML itself, only its
	// size), the modular build config and the creator's name
	virtual nlohmann::json GetPropertyModelRecords(LWOOBJID propertyId) = 0;

	// Moderators' decisions on names, so players see why one was turned down. kind: "name" (subject: the character)
	// or "pet_name" (subject: the pet)
	virtual void InsertModerationDecision(const std::string& kind, int64_t subjectId, const std::string& subject, bool approved, const std::string& reason, int64_t time) = 0;
	virtual nlohmann::json GetModerationDecisions(const std::string& kind, int64_t subjectId, uint32_t limit) = 0;
};

#endif  //!__IRELATEDDATA__H__
