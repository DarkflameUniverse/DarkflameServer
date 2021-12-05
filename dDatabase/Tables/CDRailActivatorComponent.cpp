#include "CDRailActivatorComponent.h"
#include "GeneralUtils.h"

CDRailActivatorComponentTable::CDRailActivatorComponentTable() {
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RailActivatorComponent;");
    while (!tableData.eof()) {
        CDRailActivatorComponent entry;

        entry.id = tableData.getIntField(0);

        std::string startAnimation(tableData.getStringField(1, ""));
        entry.startAnimation = GeneralUtils::ASCIIToUTF16(startAnimation);

        std::string loopAnimation(tableData.getStringField(2, ""));
        entry.loopAnimation = GeneralUtils::ASCIIToUTF16(loopAnimation);

        std::string stopAnimation(tableData.getStringField(3, ""));
        entry.stopAnimation = GeneralUtils::ASCIIToUTF16(stopAnimation);

        std::string startSound(tableData.getStringField(4, ""));
        entry.startSound = GeneralUtils::ASCIIToUTF16(startSound);

        std::string loopSound(tableData.getStringField(5, ""));
        entry.loopSound = GeneralUtils::ASCIIToUTF16(loopSound);

        std::string stopSound(tableData.getStringField(6, ""));
        entry.stopSound = GeneralUtils::ASCIIToUTF16(stopSound);

        std::string loopEffectString(tableData.getStringField(7, ""));
        entry.loopEffectID = EffectPairFromString(loopEffectString);

        entry.preconditions = tableData.getStringField(8, "-1");

        entry.playerCollision = tableData.getIntField(9, 0);

        entry.cameraLocked = tableData.getIntField(10, 0);

        std::string startEffectString(tableData.getStringField(11, ""));
        entry.startEffectID = EffectPairFromString(startEffectString);

        std::string stopEffectString(tableData.getStringField(12, ""));
        entry.stopEffectID = EffectPairFromString(stopEffectString);

        entry.damageImmune = tableData.getIntField(13, 0);

        entry.noAggro = tableData.getIntField(14, 0);

        entry.showNameBillboard = tableData.getIntField(15, 0);

        m_Entries.push_back(entry);
        tableData.nextRow();
    }

    tableData.finalize();
}

CDRailActivatorComponentTable::~CDRailActivatorComponentTable() = default;

std::string CDRailActivatorComponentTable::GetName() const {
    return "RailActivatorComponent";
}

CDRailActivatorComponent CDRailActivatorComponentTable::GetEntryByID(int32_t id) const {
    for (const auto& entry : m_Entries) {
        if (entry.id == id)
            return entry;
    }

    return {};
}

std::vector<CDRailActivatorComponent> CDRailActivatorComponentTable::GetEntries() const {
    return m_Entries;
}

std::pair<uint32_t, std::u16string> CDRailActivatorComponentTable::EffectPairFromString(std::string &str) {
    const auto split = GeneralUtils::SplitString(str, ':');
    if (split.size() == 2) {
        return { std::stoi(split.at(0)), GeneralUtils::ASCIIToUTF16(split.at(1)) };
    }

    return {};
}
