#include "CDFaceItemComponentTable.h"
#include <dCommonVars.h>

CDFaceItemComponentTable::CDFaceItemComponentTable() {
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
        entry.id = tableData.getIntField(0, 0);
        entry.eyes = tableData.getIntField(1, 0);
        entry.eyebrows = tableData.getIntField(2, 0);
        entry.mouth = tableData.getIntField(3, 0);

        this->entries.push_back(entry);
        tableData.nextRow();
    }

    tableData.finalize();
}

CDFaceItemComponentTable::~CDFaceItemComponentTable() {}

std::string CDFaceItemComponentTable::GetName() const {
    return "FaceItemComponent";
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