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
		uint32_t attempts{};
		int64_t processedAt{};      // Unix seconds of the UGC server's last attempt, 0 for none
		int64_t processAfter{};     // models: the end of the quiet period after a save
		bool bakeAo{};
		uint32_t bricks{};          // models: counted by the UGC server when it made them (0: not yet)
		uint32_t triangles{};       // models: of the made mesh's most detailed level (0: not yet)
		uint32_t processMs{};       // how long the last successful make took (0: not made, or made before it was timed)
		uint32_t processCpuMs{};    // the worker thread's CPU time for it
		uint32_t processMemoryKb{}; // the memory the UGC server estimated for it (not measured)
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

	enum class eSort : uint8_t {
		NEWEST,    // the highest id first (ids are handed out in order)
		OLDEST,
		OWNER,     // the creator's character name
		NAME,      // models: the upload's file name; modular builds: the modules
		BRICKS,    // models: the most bricks first
		TRIANGLES, // models: the most triangles first
		SLOWEST,   // the longest last make first (process_ms)
		MADE,      // the most recently made (or attempted) first (processed_at)
	};

	// A page of one kind: all of them or those matching the search (the same matching as SearchUgc), in a state or any
	struct UgcListQuery {
		UgcSearch search;
		std::optional<IUgc::eProcessState> state;
		eSort sort{ eSort::NEWEST };
		bool reverse{}; // the sort's other direction (e.g. the fewest bricks first)
		uint32_t offset{};
		uint32_t limit{ 50 };
	};

	// The page and how many match in all
	virtual std::pair<std::vector<UgcEntry>, uint64_t> ListUgc(const eUgcKind kind, const UgcListQuery& query) = 0;

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
