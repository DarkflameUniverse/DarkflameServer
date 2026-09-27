#ifndef IUGCLOOKUP_H
#define IUGCLOOKUP_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "IUgc.h"

/**
 * Finding players' creations for the dashboard: brick built models (ugc) and cars and rockets (ugc_modular_build),
 * who made them, what the UGC server made of them and where they are (placed on a property, attached to a mail).
 */
class IUgcLookup {
public:
	enum class eUgcKind : uint8_t {
		MODEL = 0,   // ugc: a brick built model, its id is the blueprint id
		MODULAR = 1, // ugc_modular_build: a car or rocket, its id is the item's subkey
	};

	struct UgcEntry {
		eUgcKind kind{};
		LWOOBJID id{};
		LWOOBJID characterId{};     // the creator
		std::string characterName;  // empty when the character is gone
		uint32_t accountId{};
		std::string accountName;
		IUgc::eProcessState state{};
		std::string error;          // why the UGC server failed, if it did
		std::string detail;         // models: the file name it was uploaded as; modular builds: the modules
	};

	// What SearchUgc matches. A number (when set) is matched against ids; text against names
	struct UgcSearch {
		enum class eField : uint8_t {
			ANY,      // any of the below
			ID,       // the ugc / blueprint id, the placed model's object id, the property, creator character or account id
			OWNER,    // the creator's character or account name (or id)
			PROPERTY, // the name (or id) of a property it is placed on
			MODEL,    // the name or description given to it on a property, or its upload file name
			LOT,      // the LOT it is placed as, or one of a modular build's modules
		};
		eField field = eField::ANY;
		std::string text;              // matched as a part of names; empty for none
		std::optional<int64_t> number; // matched exactly against ids
	};

	// Up to `limit` of each kind, the newest first
	virtual std::vector<UgcEntry> SearchUgc(const UgcSearch& search, const uint32_t limit) = 0;

	// The entries (of either kind) with these ids
	virtual std::vector<UgcEntry> GetUgcEntries(const std::vector<LWOOBJID>& ids) = 0;

	// A creation placed on a property (properties_contents)
	struct UgcPlacement {
		LWOOBJID ugcId{};
		LWOOBJID modelId{};    // the placed object's id
		LOT lot{};
		LWOOBJID propertyId{};
		std::string propertyName;
		LWOOBJID ownerId{};    // the property owner's character
		std::string ownerName;
		uint32_t zoneId{};
		std::string modelName;
		std::string modelDescription;
	};

	virtual std::vector<UgcPlacement> GetUgcPlacements(const std::vector<LWOOBJID>& ugcIds) = 0;

	// A mail with an attachment that may be a creation: its subkey is one of `subkeys` (modular builds), or it is a
	// brick built model item (`modelItemLot`, whose blueprint is in the attachment's config)
	struct UgcMail {
		uint64_t id{};
		LWOOBJID receiverId{};
		std::string receiverName;
		LOT lot{};
		LWOOBJID subkey{};
		std::string config;
	};

	virtual std::vector<UgcMail> GetUgcMail(const std::vector<LWOOBJID>& subkeys, const LOT modelItemLot) = 0;
};

#endif  //!IUGCLOOKUP_H
