#pragma once

#include "CDAbstractProvider.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <functional>
#include <utility>
#include <chrono>
#include <mutex>

template <
    typename KeyType,
    typename MappedType
>
class CDAbstractSharedMemoryProvider : public CDAbstractProvider<KeyType, MappedType>
{
    typedef std::pair<const KeyType, MappedType> ValueType;

    typedef boost::interprocess::allocator<ValueType, boost::interprocess::managed_shared_memory::segment_manager> ShmemAllocator;

    typedef boost::interprocess::map<KeyType, MappedType, std::less<KeyType>, ShmemAllocator> Map;

public:
    std::string m_Name;
    std::string m_MapName;
    std::function <ValueType(CppSQLite3Query&)> m_ParseEntry;
    std::function <int32_t(int32_t)> m_CalculateSize;

    bool m_Cache;

    std::unordered_map<KeyType, MappedType> m_CacheMap;
    
    bool m_Host;
    Map* m_HostEntries;

    boost::interprocess::managed_shared_memory m_ClientSegment;
    ShmemAllocator* m_ClientAllocInst;
    boost::interprocess::offset_ptr<Map> m_ClientEntires;

    CDAbstractSharedMemoryProvider(std::string name, std::function <ValueType(CppSQLite3Query&)> parseEntry, std::function <int32_t(int32_t)> calculateSize, bool cache)
    {
        m_Name = name;
        m_MapName = name + "Map";
        m_ParseEntry = parseEntry;
        m_CalculateSize = calculateSize;
        m_Cache = cache;
        m_Host = false;
        m_HostEntries = nullptr;

        LoadClient();
    }

    const MappedType& GetEntry(const KeyType& key, const MappedType& defaultValue) override {
        if (m_Host) {
            auto it = m_HostEntries->find(key);
            if (it == m_HostEntries->end())
            {
                return defaultValue;
            }
            return it->second;
        }

        if (m_Cache) {
            auto it = m_CacheMap.find(key);
            if (it != m_CacheMap.end()) {
                return it->second;
            }
        }

        const auto& it = m_ClientEntires->find(key);
        if (it == m_ClientEntires->end())
        {
            if (m_Cache) {
                m_CacheMap.emplace(key, defaultValue);
            }

            return defaultValue;
        }

        if (m_Cache) {
            m_CacheMap.emplace(key, it->second);
        }

        return it->second;
    }

    void LoadClient() override {
        try {
            m_ClientSegment = boost::interprocess::managed_shared_memory(boost::interprocess::open_read_only, m_MapName.c_str());

            m_ClientAllocInst = new ShmemAllocator(m_ClientSegment.get_segment_manager());

            m_ClientEntires = m_ClientSegment.find<Map>(m_Name.c_str()).first;

            if (m_ClientEntires == nullptr) {
                throw std::runtime_error("Could not find shared memory segment " + m_Name);
            }

        } catch (boost::interprocess::interprocess_exception& e) {
            // Not open
        }
    }

    void LoadHost() override {
        try {
            boost::interprocess::shared_memory_object::remove(m_MapName.c_str());

            auto sizeQuery = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM " + m_Name);

            if (sizeQuery.eof()) {
                throw std::runtime_error("Could not get size of table " + m_Name);
                return;
            }

            int32_t size = sizeQuery.getIntField(0);

            size = m_CalculateSize(size);

            boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only, m_MapName.c_str(), size);

            ShmemAllocator alloc_inst (segment.get_segment_manager());

            m_HostEntries = segment.construct<Map>(m_Name.c_str()) (std::less<KeyType>(), alloc_inst);

            auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM " + m_Name);

            while (!tableData.eof()) {
                ValueType entry = m_ParseEntry(tableData);

                m_HostEntries->insert(entry);

                tableData.nextRow();
            }

            tableData.finalize();

            m_Host = true;

            LoadClient();
        } catch (std::exception &e) {
            // Make sure the smemory is removed
            boost::interprocess::shared_memory_object::remove(m_MapName.c_str());

            throw e;
        }
    }
};
