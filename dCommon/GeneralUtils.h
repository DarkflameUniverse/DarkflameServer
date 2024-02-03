#pragma once

// C++
#include <charconv>
#include <stdint.h>
#include <random>
#include <time.h>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <type_traits>
#include <stdexcept>
#include "BitStream.h"
#include "NiPoint3.h"

#include "Game.h"
#include "Logger.h"

enum eInventoryType : uint32_t;
enum class eObjectBits : size_t;
enum class eReplicaComponentType : uint32_t;

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
	std::u16string ASCIIToUTF16(const std::string_view& string, size_t size = -1);

	//! Converts a UTF-8 String to a UTF-16 string
	/*!
	  \param string The string to convert
	  \param size A size to trim the string to. Default is -1 (No trimming)
	  \return An UTF-16 representation of the string
	 */
	std::u16string UTF8ToUTF16(const std::string_view& string, size_t size = -1);

	//! Internal, do not use
	bool _NextUTF8Char(std::string_view& slice, uint32_t& out);

	//! Converts a UTF-16 string to a UTF-8 string
	/*!
	  \param string The string to convert
	  \param size A size to trim the string to. Default is -1 (No trimming)
	  \return An UTF-8 representation of the string
	 */
	std::string UTF16ToWTF8(const std::u16string_view& string, size_t size = -1);

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
	inline void SetBit(T& value, eObjectBits bits) {
		static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
		auto index = static_cast<size_t>(bits);
		if (index > (sizeof(T) * 8) - 1) {
			return;
		}

		value |= static_cast<T>(1) << index;
	}

	//! Clears a bit on a numerical value
	template <typename T>
	inline void ClearBit(T& value, eObjectBits bits) {
		static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
		auto index = static_cast<size_t>(bits);
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

	bool ReplaceInString(std::string& str, const std::string& from, const std::string& to);

	std::u16string ReadWString(RakNet::BitStream* inStream);

	std::vector<std::wstring> SplitString(std::wstring& str, wchar_t delimiter);

	std::vector<std::u16string> SplitString(const std::u16string& str, char16_t delimiter);

	std::vector<std::string> SplitString(const std::string& str, char delimiter);

	std::vector<std::string> GetSqlFileNamesFromFolder(const std::string& folder);

	// Concept constraining to enum types
	template <typename T>
	concept Enum = std::is_enum_v<T>;

	// Concept constraining to numeric types
	template <typename T>
	concept Numeric = std::integral<T> || Enum<T> || std::floating_point<T>;

	// Concept trickery to enable parsing underlying numeric types
	template <Numeric T>
	struct numeric_parse { using type = T; };

	// If an enum, present an alias to its underlying type for parsing
	template <Numeric T> requires Enum<T>
	struct numeric_parse<T> { using type = std::underlying_type_t<T>; };

	// If a boolean, present an alias to an integral type for parsing
	template <Numeric T> requires std::same_as<T, bool>
	struct numeric_parse<T> { using type = uint32_t; };

	// Shorthand type alias
	template <Numeric T>
	using numeric_parse_t = numeric_parse<T>::type;

	/**
	 * For numeric values: Parses a C-style char range (string) and returns an optional variable depending on the result
	 * @param str The pointer to the start of the char range (string)
	 * @param strEnd The pointer to the end of the char range (string), defaults to NULL
	 * @returns An std::optional containing the desired value if it exists in the string
	*/
	template <Numeric T>
	[[nodiscard]] std::optional<T> TryParse(const char* const str, const char* const strEnd = NULL) noexcept {
		numeric_parse_t<T> result;
		const bool isParsed = std::from_chars(str, strEnd, result).ec == std::errc{};

		return isParsed ? static_cast<T>(result) : std::optional<T>{};
	}

	/**
	 * For floating-point values: Parses a C-style char range (string) and returns an optional variable depending on the result
	 * @param str The pointer to the start of the char range (string)
	 * @param strEnd The pointer to the end of the char range (string), defaults to NULL
	 * @returns An std::optional containing the desired value if it exists in the string
	*/
	/*template <std::floating_point T>
	[[nodiscard]] std::optional<T> TryParse(const char* const str, const char* const strEnd = NULL) noexcept
	try {
		return std::make_optional<T>(std::stold(str));
	} catch (...) {
		return std::nullopt;
	}*/

	/**
	 * The TryParse overload for std::string
	 * @param str A constant reference to a std::string
	 * @returns An std::optional containing the desired value if it exists in the string
	*/
	template <typename T>
	[[nodiscard]] std::optional<T> TryParse(const std::string& str) noexcept {
		return TryParse<T>(str.data(), str.data() + str.size());
	}

	/**
	 * The TryParse overload for std::string_view
	 * @param str A constant value std::string_view passed by copy
	 * @returns An std::optional containing the desired value if it exists in the string
	*/
	template <typename T>
	[[nodiscard]] std::optional<T> TryParse(const std::string_view str) noexcept {
		return TryParse<T>(str.data(), str.data() + str.size());
	}

	/**
	 * The TryParse overload for handling NiPoint3 by passing 3 seperate string references
	 * @param strX The string representing the X coordinate
	 * @param strY The string representing the Y coordinate
	 * @param strZ The string representing the Z coordinate
	 * @returns An std::optional containing the desired NiPoint3 if it can be constructed from the string parameters
	*/
	template <typename T>
	[[nodiscard]] std::optional<NiPoint3> TryParse(const std::string& strX, const std::string& strY, const std::string& strZ) noexcept {
		const auto x = TryParse<float>(strX);
		const auto y = TryParse<float>(strY);
		const auto z = TryParse<float>(strZ);

		return x && y && z ? std::make_optional<NiPoint3>(x.value(), y.value(), z.value()) : std::nullopt;
	}

	/**
	 * The TryParse overload for handling NiPoint3 by passingn a reference to a vector of three strings
	 * @param str The string vector representing the X, Y, and Xcoordinates
	 * @returns An std::optional containing the desired NiPoint3 if it can be constructed from the string parameters
	*/
	template <typename T>
	[[nodiscard]] std::optional<NiPoint3> TryParse(const std::vector<std::string>& str) noexcept {
		return TryParse<NiPoint3>(str.at(0), str.at(1), str.at(2));
	}

	template <typename T>
	std::u16string to_u16string(T value) {
		return GeneralUtils::ASCIIToUTF16(std::to_string(value));
	}

	// From boost::hash_combine
	template <class T>
	constexpr void hash_combine(std::size_t& s, const T& v) noexcept {
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}

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
		} else if (std::is_floating_point_v<T>) {
			std::uniform_real_distribution<T> distribution(min, max);
			return distribution(Game::randomEngine);
		}

		return T();
	}

	/**
	 * Casts the value of an enum entry to its underlying type
	 * @param entry Enum entry to cast
	 * @returns The enum entry's value in its underlying type
	*/
	template <Enum eType>
	constexpr typename std::underlying_type_t<eType> CastUnderlyingType(const eType entry) noexcept {
		return static_cast<typename std::underlying_type_t<eType>>(entry);
	}

	// on Windows we need to undef these or else they conflict with our numeric limits calls
	// DEVELOPERS DEVELOPERS DEVELOPERS DEVELOPERS DEVELOPERS DEVELOPERS DEVELOPERS DEVELOPERS
	#ifdef _WIN32
	#undef min
	#undef max
	#endif

	template <typename T>
	inline T GenerateRandomNumber() {
		// Make sure it is a numeric type
		static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");

		return GenerateRandomNumber<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	}
}
