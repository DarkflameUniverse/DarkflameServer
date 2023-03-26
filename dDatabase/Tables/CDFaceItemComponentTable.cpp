#include "CDFaceItemComponentTable.h"

CDFaceItemComponentTable::CDFaceItemComponentTable(void) {
    unsigned int size = 0;
    auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM FaceItemComponent");
    while (!tableSize.eof()) {
        size = tableSize.getIntField(0, 0);

        tableSize.nextRow();
    }

    tableSize.finalize();
    this->entries.reserve(size);
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM FaceItemComponent");
    while (!tableData.eof()) {
        CDFaceItemComponent entry;
        entry.id = tableData.getIntField("id", 0);
        entry.eyes = tableData.getIntField("eyes", 0);
        entry.eyebrows = tableData.getIntField("eyebrows", 0);
        entry.mouth = tableData.getIntField("mouths", 0);

        this->entries.push_back(entry);
        tableData.nextRow();
    }

    tableData.finalize();
}

std::vector<CDFaceItemComponent> CDFaceItemComponentTable::Query(std::function<bool(CDFaceItemComponent)> predicate) {

    std::vector<CDFaceItemComponent> data = cpplinq::from(this->entries) >> cpplinq::where(predicate) >> cpplinq::to_vector();

    return data;
}

std::vector<CDFaceItemComponent> CDFaceItemComponentTable::GetEntries(void) const {
    return this->entries;
}

CDFaceItemComponent CDFaceItemComponentTable::GetByLot(LOT lot) {
    for (const auto& item : entries) {
        if (item.id == lot) {
            return item;
        }
    }

    return {};
}

CDFaceItemComponent CDFaceItemComponentTable::GetByEyes(uint32_t id) {
    for (const auto& item : entries) {
        if (item.eyes == id) {
            return item;
        }
    }

    return {};
}

CDFaceItemComponent CDFaceItemComponentTable::GetByEyebrows(uint32_t id) {
    for (const auto& item : entries) {
        if (item.eyebrows == id) {
            return item;
        }
    }

    return {};
}

CDFaceItemComponent CDFaceItemComponentTable::GetByMouth(uint32_t id) {
    for (const auto& item : entries) {
        if (item.mouth == id) {
            return item;
        }
    }

    return {};
}
