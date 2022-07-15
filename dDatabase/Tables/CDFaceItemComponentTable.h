#pragma once

#include "CDTable.h"

struct CDFaceItemComponent {
    int32_t id;
    int32_t eyes;
    int32_t eyebrows;
    int32_t mouth;
};

class CDFaceItemComponentTable : public CDTable {
  private:
    std::vector<CDFaceItemComponent> entries;

  public:
    CDFaceItemComponentTable();
    ~CDFaceItemComponentTable();

    std::string GetName() const override;

    CDFaceItemComponent GetByLot(LOT lot);

    CDFaceItemComponent GetByEyes(uint32_t id);
    CDFaceItemComponent GetByEyebrows(uint32_t id);
    CDFaceItemComponent GetByMouth(uint32_t id);

    std::vector<CDFaceItemComponent> Query(std::function<bool(CDFaceItemComponent)> predicate);

    std::vector<CDFaceItemComponent> GetEntries(void) const;
};
