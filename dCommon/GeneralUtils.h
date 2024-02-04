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

#include "dPlatforms.h"
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

	// If a boolean, present an alias to an intermediate integral type for parsing
	template <Numeric T> requires std::same_as<T, bool>
	struct numeric_parse<T> { using type = uint32_t; };

	// Shorthand type alias
	template <Numeric T>
	using numeric_parse_t = numeric_parse<T>::type;

	/**
	 * For numeric values: Parses a string_view and returns an optional variable depending on the result.
	 * @param str The string_view to be evaluated
	 * @returns An std::optional containing the desired value if it is equivalent to the string
	*/
	template <Numeric T>
	[[nodiscard]] std::optional<T> TryParse(const std::string_view str) {
		numeric_parse_t<T> result;

		const char* const strEnd = str.data() + str.size();
		const auto [parseEnd, ec] = std::from_chars(str.data(), strEnd, result);
		const bool isParsed = parseEnd == strEnd && ec == std::errc{};

		return isParsed ? static_cast<T>(result) : std::optional<T>{};
	}

#ifdef DARKFLAME_PLATFORM_MACOS

	// Anonymous namespace containing MacOS floating-point parse function specializations
	namespace {
		template <std::floating_point T>
		[[nodiscard]] T Parse(const std::string_view str, size_t* parseNum);

		template <>
		[[nodiscard]] float Parse<float>(const std::string_view str, size_t* parseNum) {
			return std::stof(std::string{ str }, parseNum);
		}

		template <>
		[[nodiscard]] double Parse<double>(const std::string_view str, size_t* parseNum) {
			return std::stod(std::string{ str }, parseNum);
		}

		template <>
		[[nodiscard]] long double Parse<long double>(const std::string_view str, size_t* parseNum) {
			return std::stold(std::string{ str }, parseNum);
		}
	}

	/**
	 * For floating-point values: Parses a string_view and returns an optional variable depending on the result.
	 * Note that this function overload is only included for MacOS, as from_chars will fulfill its purpose otherwise.
	 * @param str The string_view to be evaluated
	 * @returns An std::optional containing the desired value if it is equivalent to the string
	*/
	template <std::floating_point T>
	[[nodiscard]] std::optional<T> TryParse(const std::string_view str) noexcept try {
		size_t parseNum;
		const T result = Parse<T>(str, &parseNum);
		const bool isParsed = str.length() == parseNum;

		return isParsed ? result : std::optional<T>{};
	} catch (...) {
		return std::nullopt;
	}

#endif

	/**
	 * The TryParse overload for handling NiPoint3 by passing 3 seperate string references
	 * @param strX The string representing the X coordinate
	 * @param strY The string representing the Y coordinate
	 * @param strZ The string representing the Z coordinate
	 * @returns An std::optional containing the desired NiPoint3 if it can be constructed from the string parameters
	*/
	template <typename T>
	[[nodiscard]] std::optional<NiPoint3> TryParse(const std::string& strX, const std::string& strY, const std::string& strZ) {
		const auto x = TryParse<float>(strX);
		if (!x) return std::nullopt;

		const auto y = TryParse<float>(strY);
		if (!y) return std::nullopt;

		const auto z = TryParse<float>(strZ);
		return z ? std::make_optional<NiPoint3>(x.value(), y.value(), z.value()) : std::nullopt;
	}

	/**
	 * The TryParse overload for handling NiPoint3 by passingn a reference to a vector of three strings
	 * @param str The string vector representing the X, Y, and Xcoordinates
	 * @returns An std::optional containing the desired NiPoint3 if it can be constructed from the string parameters
	*/
	template <typename T>
	[[nodiscard]] std::optional<NiPoint3> TryParse(const std::vector<std::string>& str) {
		return (str.size() == 3) ? TryParse<NiPoint3>(str[0], str[1], str[2]) : std::nullopt;
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
