#pragma once

#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"

#include "CDTable.h"

#include "CDAbstractProvider.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <functional>
#include <utility>
#include <chrono>

template <
    typename KeyType,
    typename MappedType
>
class CDAbstractSharedMemoryMap : public CDAbstractProvider<KeyType, MappedType>
{
    typedef std::pair<const KeyType, MappedType> ValueType;

    typedef boost::interprocess::allocator<ValueType, boost::interprocess::managed_shared_memory::segment_manager> ShmemAllocator;

    typedef boost::interprocess::map<KeyType, MappedType, std::less<KeyType>, ShmemAllocator> Map;

public:
    std::map<KeyType, MappedType> m_CacheMap;

    std::string m_Name;

    size_t m_Size;

    bool m_Host;
    Map* m_HostEntries;

    boost::interprocess::managed_shared_memory m_ClientSegment;
    ShmemAllocator* m_ClientAllocInst;
    boost::interprocess::offset_ptr<Map> m_ClientEntires;

    CDAbstractSharedMemoryMap(std::string name, size_t size)
    {
        m_Name = name;
        m_Size = size;
        m_Host = false;
        m_HostEntries = nullptr;
        m_ClientAllocInst = nullptr;
        m_ClientEntires = nullptr;

        LoadClient();
    }
    
    const MappedType& GetEntry(const KeyType& key, const MappedType& defaultValue) override {
        const auto& cacheIt = m_CacheMap.find(key);
        if (cacheIt != m_CacheMap.end()) {
            return cacheIt->second;
        }

        const auto& it = m_ClientEntires->find(key);
        if (it == m_ClientEntires->end()) {
            return defaultValue;
        }

        return it->second;
    }

    const void SetEntry(const KeyType& key, const MappedType& value) {
        if (m_Host) {
            // If we are already hosting, we cannot add to the map, throw an error
            throw std::runtime_error("Can not add to a map that is already being hosted");
        }

        m_CacheMap.emplace(key, value);
    }

    void LoadClient() override {
        try {
            m_ClientSegment = boost::interprocess::managed_shared_memory(boost::interprocess::open_read_only, m_Name.c_str());

            m_ClientAllocInst = new ShmemAllocator(m_ClientSegment.get_segment_manager());

            m_ClientEntires = m_ClientSegment.find<Map>(m_Name.c_str()).first;

            if (m_ClientEntires == nullptr) {
                throw std::runtime_error("Could not find shared memory segment " + m_Name);
            }

        } catch (std::exception &e) {
            // Not open
        }
    }

    void LoadHost() override {
        try {
            boost::interprocess::shared_memory_object::remove(m_Name.c_str());

            boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only, m_Name.c_str(), m_Size);

            ShmemAllocator alloc(segment.get_segment_manager());

            m_HostEntries = segment.construct<Map>(m_Name.c_str()) (std::less<KeyType>(), alloc);

            // Copy cache
            for (const auto& pair : m_CacheMap) {
                m_HostEntries->insert(std::make_pair(pair.first, pair.second));
            }

            m_Host = true;

            LoadClient();
        } catch (std::exception &e) {
            // Make sure the smemory is removed
            boost::interprocess::shared_memory_object::remove(m_Name.c_str());

            throw e;
        }
    }
};
