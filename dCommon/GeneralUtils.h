#pragma once

// C++
#include <stdint.h>
#include <random>
#include <time.h>
#include <string>
#include <type_traits>
#include <functional>
#include <type_traits>
#include <stdexcept>
#include <BitStream.h>

#include "Game.h"

/*!
  \file GeneralUtils.hpp
  \brief A namespace containing general utility functions
 */

//! The general utils namespace
namespace GeneralUtils {
    //! Converts a plain ASCII string to a UTF-16 string
    /*!
      \param string The string to convert
      \param size A size to trim the string to. Default is -1 (No trimming)
      \return An UTF-16 representation of the string
     */
    std::u16string ASCIIToUTF16(const std::string& string, size_t size = -1);

    //! Converts a UTF-16 string to a UTF-8 string
    /*!
      \param string The string to convert
      \param size A size to trim the string to. Default is -1 (No trimming)
      \return An UTF-8 representation of the string
     */
    std::string UTF16ToWTF8(const std::u16string& string, size_t size = -1);

    /**
     * Compares two basic strings but does so ignoring case sensitivity
     * \param a the first string to compare against the second string
     * \param b the second string to compare against the first string
     * @return if the two strings are equal
     */
    bool CaseInsensitiveStringCompare(const std::string& a, const std::string& b);

    // MARK: Bits

    // MARK: Bits

    //! Sets a bit on a numerical value
    template <typename T>
    void SetBit(T& value, size_t index) {
        static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
        
        if (index > (sizeof(T) * 8) - 1) {
            return;
        }
        
        value |= static_cast<T>(1) << index;
    }
    
    //! Clears a bit on a numerical value
    template <typename T>
    void ClearBit(T& value, size_t index) {
        static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
        
        if (index > (sizeof(T) * 8 - 1)) {
            return;
        }
        
        value &= ~(static_cast<T>(1) << index);
    }
    
    //! Sets a specific bit in a signed 64-bit integer
    /*!
      \param value The value to set the bit for
      \param index The index of the bit
     */
    int64_t SetBit(int64_t value, uint32_t index);
    
    //! Clears a specific bit in a signed 64-bit integer
    /*!
      \param value The value to clear the bit from
      \param index The index of the bit
     */
    int64_t ClearBit(int64_t value, uint32_t index);
    
    //! Checks a specific bit in a signed 64-bit integer
    /*!
      \parma value The value to check the bit in
      \param index The index of the bit
      \return Whether or not the bit is set
     */
    bool CheckBit(int64_t value, uint32_t index);

    // MARK: Random Number Generation

    //! Generates a random number
    /*!
      \param min The minimum the generate from
      \param max The maximum to generate to
     */
    template <typename T>
    inline T GenerateRandomNumber(std::size_t min, std::size_t max) {
        // Make sure it is a numeric type
        static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");

        if constexpr (std::is_integral_v<T>) {  // constexpr only necessary on first statement
            std::uniform_int_distribution<T> distribution(min, max);
            return distribution(Game::randomEngine);
        }
        else if (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> distribution(min, max);
            return distribution(Game::randomEngine);
        }

        return T();
    }
    
	bool ReplaceInString(std::string& str, const std::string& from, const std::string& to);

    std::u16string ReadWString(RakNet::BitStream *inStream);

	std::vector<std::wstring> SplitString(std::wstring& str, wchar_t delimiter);

	std::vector<std::u16string> SplitString(std::u16string& str, char16_t delimiter);

	std::vector<std::string> SplitString(const std::string& str, char delimiter);

    std::vector<std::string> GetFileNamesFromFolder(const std::string& folder);

    template <typename T>
    T Parse(const char* value);

	template <>
    inline int32_t Parse(const char* value)
	{
        return std::stoi(value);
	}

    template <>
    inline int64_t Parse(const char* value)
    {
        return std::stoll(value);
    }

    template <>
    inline float Parse(const char* value)
    {
        return std::stof(value);
    }

    template <>
    inline double Parse(const char* value)
    {
        return std::stod(value);
    }
	
    template <>
    inline uint32_t Parse(const char* value)
    {
        return std::stoul(value);
    }

    template <>
    inline uint64_t Parse(const char* value)
    {
        return std::stoull(value);
    }
	
    template <typename T>
    bool TryParse(const char* value, T& dst)
    {
        try
        {
            dst = Parse<T>(value);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template <typename T>
    T Parse(const std::string& value)
    {
        return Parse<T>(value.c_str());
    }

    template <typename T>
    bool TryParse(const std::string& value, T& dst)
    {
        return TryParse<T>(value.c_str(), dst);
    }

    template<typename T>
    std::u16string to_u16string(T value)
    {
        return GeneralUtils::ASCIIToUTF16(std::to_string(value));
    }

    // From boost::hash_combine
    template <class T>
    void hash_combine(std::size_t& s, const T& v)
    {
        std::hash<T> h;
        s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
    }
}
