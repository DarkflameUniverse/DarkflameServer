#include "CDTable.h"

#if defined(CD_PROVIDER_MEMORY)

#include "CDAbstractSharedMemoryMap.h"

typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator; 
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> my_string;

CDAbstractSharedMemoryMap<size_t, boost::interprocess::string>* CDStringMap;

void CDTable::InitalizeHost()
{
    CDStringMap->LoadHost();
}

void CDTable::Initalize()
{
    CDStringMap = new CDAbstractSharedMemoryMap<size_t, boost::interprocess::string>("CDStringMap", 10 * 1000 * 1000);
}

std::string CDTable::GetString(size_t handle)
{
    std::string str = std::string(CDStringMap->GetEntry(handle, "").c_str());

    return str;
}

size_t CDTable::SetString(std::string value)
{
    size_t hash = 0;

    GeneralUtils::hash_combine(hash, value);

    CDStringMap->SetEntry(hash, boost::interprocess::string(value.c_str()));

    return hash;
}

#else

std::unordered_map<size_t, std::string> CDStringMap;

void CDTable::InitalizeHost()
{
}

void CDTable::Initalize()
{
}

std::string CDTable::GetString(size_t handle)
{
    return CDStringMap[handle];
}

size_t CDTable::SetString(std::string value)
{
    size_t hash = 0;

    GeneralUtils::hash_combine(hash, value);

    CDStringMap[hash] = value;

    return hash;
}

#endif