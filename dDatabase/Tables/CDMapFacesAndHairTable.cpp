#include "CDMapFacesAndHairTable.h"

CDMapFacesAndHairTable::CDMapFacesAndHairTable(void) {
    unsigned int size = 0;
    auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM mapFacesAndHair");
    while (!tableSize.eof()) {
        size = tableSize.getIntField(0, 0);

        tableSize.nextRow();
    }

    tableSize.finalize();
    this->entries.reserve(size);
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM mapFacesAndHair");
    while (!tableData.eof()) {
        CDMapFacesAndHair entry;
        entry.id = tableData.getIntField("id", 0);
        entry.eyes = tableData.getIntField("eyes", 0);
        entry.eyebrows = tableData.getIntField("eyebrows", 0);
        entry.mouth = tableData.getIntField("mouths", 0);
		entry.haircolor = tableData.getIntField("haircolor", 0);
		entry.hairstyle = tableData.getIntField("hairstyle", 0);

        this->entries.push_back(entry);
        tableData.nextRow();
    }

    tableData.finalize();
}

std::vector<CDMapFacesAndHair> CDMapFacesAndHairTable::Query(std::function<bool(CDMapFacesAndHair)> predicate) {

    std::vector<CDMapFacesAndHair> data = cpplinq::from(this->entries) >> cpplinq::where(predicate) >> cpplinq::to_vector();

    return data;
}

std::vector<CDMapFacesAndHair> CDMapFacesAndHairTable::GetEntries(void) const {
    return this->entries;
}

CDMapFacesAndHair CDMapFacesAndHairTable::GetByLot(LOT lot) {
    for (const auto& item : entries) {
        if (item.id == lot) {
            return item;
        }
    }

    return {};
}

CDMapFacesAndHair CDMapFacesAndHairTable::GetByEyes(uint32_t id) {
    for (const auto& item : entries) {
        if (item.eyes == id) {
            return item;
        }
    }

    return {};
}

CDMapFacesAndHair CDMapFacesAndHairTable::GetByEyebrows(uint32_t id) {
    for (const auto& item : entries) {
        if (item.eyebrows == id) {
            return item;
        }
    }

    return {};
}

CDMapFacesAndHair CDMapFacesAndHairTable::GetByMouth(uint32_t id) {
    for (const auto& item : entries) {
        if (item.mouth == id) {
            return item;
        }
    }

    return {};
}
