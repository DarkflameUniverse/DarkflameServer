#pragma once

#include "CDTable.h"
#include "dCommonVars.h"

struct CDMapFacesAndHair {
    int32_t id;
    int32_t eyes;
    int32_t eyebrows;
    int32_t mouth;
    int32_t haircolor;
    int32_t hairstyle;
};

class CDMapFacesAndHairTable : public CDTable<CDMapFacesAndHairTable> {
  private:
    std::vector<CDMapFacesAndHair> entries;

  public:
	void LoadValuesFromDatabase();

    CDMapFacesAndHair GetByLot(LOT lot);
    CDMapFacesAndHair GetByEyes(uint32_t id);
    CDMapFacesAndHair GetByEyebrows(uint32_t id);
    CDMapFacesAndHair GetByMouth(uint32_t id);

    std::vector<CDMapFacesAndHair> Query(std::function<bool(CDMapFacesAndHair)> predicate);

    std::vector<CDMapFacesAndHair> GetEntries(void) const;
};
