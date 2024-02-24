#ifndef __CDMOVINGPLATFORMCOMPONENTTABLE__H__
#define __CDMOVINGPLATFORMCOMPONENTTABLE__H__

#include "CDTable.h"
#include "NiPoint3.h"

#include <optional>

typedef uint32_t ComponentID;

struct CDMovingPlatformTableEntry {
	NiPoint3 platformMove;
	float moveTime;
	bool platformIsSimpleMover;
	bool platformStartAtEnd;
};

class CDMovingPlatformComponentTable : public CDTable<CDMovingPlatformComponentTable, std::map<ComponentID, CDMovingPlatformTableEntry>> {
public:
	CDMovingPlatformComponentTable();

	void CachePlatformEntry(ComponentID id);
	const std::optional<CDMovingPlatformTableEntry> GetPlatformEntry(ComponentID id);
};
#endif  //!__CDMOVINGPLATFORMCOMPONENTTABLE__H__
