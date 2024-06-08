#include "Lookup.h"

#include <fstream>

#include "json.hpp"

#include <iostream>

using namespace nejlika;

Lookup::Lookup(const std::filesystem::path& lookup)
{
    m_Lookup = lookup;

    // Check if the file exists.
    if (!std::filesystem::exists(lookup))
    {
        // Empty lookup
        return;
    }

    // Read the json file
    std::ifstream file(lookup);

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Parse the json
    auto doc = nlohmann::json::parse(json);


    // The document is a map, so we can iterate over it.
    for (const auto& [key, v] : doc.items()) {
        if (v.is_number_integer())
        {
            m_LookupMap[key] = v.get<int64_t>();

            continue;
        }
        
        if (!v.is_object()) {
            std::stringstream ss;

            ss << "Invalid value for key \"" << key << "\" in lookup.";

            throw std::runtime_error(ss.str());
        }

        // Get the data
        auto data = v.get<nlohmann::json>();

        // Get the id
        auto id = data["id"].get<int64_t>();

        m_LookupMap[key] = id;

        // Get the metadata
        auto metadata = data["metadata"].get<std::string>();

        m_Metadata[key] = metadata;
    }
}

nejlika::Lookup::Lookup(const Lookup &other)
{
    m_Lookup = other.m_Lookup;
    m_LookupMap = other.m_LookupMap;
    m_Metadata = other.m_Metadata;
    m_CoreSymbols = other.m_CoreSymbols;
}

id Lookup::GetValue(const name& symbol) const
{
    id value;

    if (IsCoreSymbol(symbol, value)) {
        return value;
    }

    const auto& it = m_LookupMap.find(symbol);

    if (it == m_LookupMap.end())
    {
        std::stringstream ss;

        ss << "Symbol \"" << symbol << "\" does not exist in the lookup.";

        throw std::runtime_error(ss.str());
    }

    return it->second;
}

bool nejlika::Lookup::Exists(const name &symbol) const
{
    return IsCoreSymbol(symbol) || (m_CoreSymbols.find(symbol) != m_CoreSymbols.end()) || (m_LookupMap.find(symbol) != m_LookupMap.end());
}

bool nejlika::Lookup::Exists(id value) const
{
    for (const auto& [k, v] : m_LookupMap)
    {
        if (v == value)
        {
            return true;
        }
    }

    return false;
}

const std::string& Lookup::GetMetadata(const name& symbol) const
{
    const auto& it = m_Metadata.find(symbol);

    if (it == m_Metadata.end())
    {
        static std::string empty;

        return empty;
    }

    return it->second;
}

const std::filesystem::path& Lookup::GetLookup() const
{
    return m_Lookup;
}

const std::unordered_map<name, id> &nejlika::Lookup::GetMap() const
{
    return m_LookupMap;
}

bool nejlika::Lookup::IsCoreSymbol(const name &symbol, id &value) const
{
    try {
        value = std::stoi(symbol);

        return true;
    } catch (...) {
        // cont...
    }

    if (!symbol.starts_with(core_prefix)) {
        return false;
    }

    // Check in the core symbols
    const auto& it = m_CoreSymbols.find(symbol);

    if (it != m_CoreSymbols.end())
    {
        value = it->second;

        return true;
    }
    
    // In the format "core:<value>"
    try {
        value = std::stoi(symbol.substr(core_prefix.size() + 1));
    } catch (...) {
        return false;
    }

    return true;
}

bool nejlika::Lookup::IsCoreSymbol(const name &symbol)
{
    // Check if it can be converted to an integer
    try {
        [[maybe_unused]] auto value = std::stoi(symbol);

        return true;
    } catch (...) {
        // cont...
    }
    
    return symbol.starts_with(core_prefix);
}
