#pragma once

#include <filesystem>
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace nejlika {

typedef std::string name;
typedef int id;

/**
 * @brief A one-way mapping between symbols (names) and their corresponding numerical values.
 * 
 * This is an active lookup, meaning that it is possible to add new symbols and their values to the lookup.
 * 
 * A method is provided to wait for a symbol to be added to the lookup.
 */
class Lookup
{
public:
    inline static const name core_prefix = "lego-universe";

    /**
     * @brief Constructs a Lookup object with the specified lookup file path.
     *
     * @param lookup The path to the lookup file.
     * @throw If the lookup file could not be parsed.
     */
    Lookup(const std::filesystem::path& lookup);

    Lookup(const Lookup& other);

    Lookup() = default;

    /**
     * @brief Gets the value of the specified symbol.
     *
     * @param symbol The symbol to get the value of.
     * @return The value of the specified symbol.
     * @throw If the specified symbol does not exist.
     */
    id GetValue(const name& symbol) const;

    /**
     * @brief Checks whether the specified symbol exists.
     * 
     * @param symbol The symbol to check.
     * @return Whether the specified symbol exists.
     */
    bool Exists(const name& symbol) const;
    
    /**
     * @brief Checks whether any symbol has the specified value.
     * 
     * @param value The value to check.
     * @return Whether any symbol has the specified value.
     */
    bool Exists(id value) const;

    /**
     * @brief Gets the metadata of a specified symbol.
     * 
     * @param symbol The symbol to get metadata of.
     * @return The metadata of the specified symbol or an empty string if the symbol does not exist.
     */
    const std::string& GetMetadata(const name& symbol) const;

    /**
     * @brief Gets the path to the lookup file.
     *
     * @return The path to the lookup file.
     */
    const std::filesystem::path& GetLookup() const;

    /**
     * @brief Gets the map of all symbols and their values.
     * 
     * @return The map of all symbols and their values.
     */
    const std::unordered_map<name, id>& GetMap() const;

    /**
     * @brief Checks whether the specified symbol is a core symbol.
     * 
     * @param symbol The symbol to check.
     * @param value The value of the core symbol.
     * @return Whether the specified symbol is a core symbol.
     */
    bool IsCoreSymbol(const name& symbol, id& value) const;

    /**
     * @brief Checks whether the specified symbol is a core symbol.
     * 
     * A symbol is considered a core symbol if it is either:
     * a number;
     * or a string starting with the core_prefix.
     * 
     * @param symbol The symbol to check.
     * @return Whether the specified symbol is a core symbol.
     */
    static bool IsCoreSymbol(const name& symbol);

private:

    std::filesystem::path m_Lookup;

    std::unordered_map<name, id> m_LookupMap;

    std::unordered_map<name, std::string> m_Metadata;

    std::unordered_map<name, id> m_CoreSymbols;
};

} // namespace nejlika