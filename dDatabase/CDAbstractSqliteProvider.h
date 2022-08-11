#pragma once

#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"

#include "CDTable.h"

#include "CDAbstractProvider.h"

template <
    typename KeyType,
    typename MappedType
>
class CDAbstractSqliteProvider : public CDAbstractProvider<KeyType, MappedType>
{
    typedef std::pair<const KeyType, MappedType> ValueType;

public:
    std::string m_Name;
    std::function <ValueType(CppSQLite3Query&)> m_ParseEntry;

    std::unordered_map<KeyType, MappedType> m_Entries;

    CDAbstractSqliteProvider(std::string name, std::function <ValueType(CppSQLite3Query&)> parseEntry, bool cache)
    {
        m_Name = name;
        m_ParseEntry = parseEntry;

        LoadClient();
    }

    void LoadClient() override {
        // First, get the size of the table
        uint32_t size = 0;
        auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM " + m_Name);
        while (!tableSize.eof()) {
            size = tableSize.getIntField(0, 0);

            tableSize.nextRow();
        }

        tableSize.finalize();

        // Reserve the size
        m_Entries.reserve(size);

        // Now get the data
        auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM " + m_Name);
        while (!tableData.eof()) {
            auto entry = m_ParseEntry(tableData);

            m_Entries.insert(entry);

            tableData.nextRow();
        }

        tableData.finalize();
    }

    void LoadHost() override {
        return;
    }

    const MappedType& GetEntry(const KeyType& key, const MappedType& defaultValue) override {
        auto it = m_Entries.find(key);
        if (it != m_Entries.end()) {
            return it->second;
        }
        return defaultValue;
    }
};
