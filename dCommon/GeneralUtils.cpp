#include "GeneralUtils.h"

// C++
#include <cstdint>
#include <cassert>
#include <algorithm>

template <typename T>
inline size_t MinSize(size_t size, const std::basic_string_view<T>& string) {
	if (size == size_t(-1) || size > string.size()) {
		return string.size();
	} else {
		return size;
	}
}

inline bool IsLeadSurrogate(char16_t c) {
	return (0xD800 <= c) && (c <= 0xDBFF);
}

inline bool IsTrailSurrogate(char16_t c) {
	return (0xDC00 <= c) && (c <= 0xDFFF);
}

inline void PushUTF8CodePoint(std::string& ret, char32_t cp) {
	if (cp <= 0x007F) {
		ret.push_back(static_cast<uint8_t>(cp));
	} else if (cp <= 0x07FF) {
		ret.push_back(0xC0 | (cp >> 6));
		ret.push_back(0x80 | (cp & 0x3F));
	} else if (cp <= 0xFFFF) {
		ret.push_back(0xE0 | (cp >> 12));
		ret.push_back(0x80 | ((cp >> 6) & 0x3F));
		ret.push_back(0x80 | (cp & 0x3F));
	} else if (cp <= 0x10FFFF) {
		ret.push_back(0xF0 | (cp >> 18));
		ret.push_back(0x80 | ((cp >> 12) & 0x3F));
		ret.push_back(0x80 | ((cp >> 6) & 0x3F));
		ret.push_back(0x80 | (cp & 0x3F));
	} else {
		assert(false);
	}
}

constexpr const char16_t REPLACEMENT_CHARACTER = 0xFFFD;

bool _IsSuffixChar(uint8_t c) {
	return (c & 0xC0) == 0x80;
}

bool GeneralUtils::_NextUTF8Char(std::string_view& slice, uint32_t& out) {
	size_t rem = slice.length();
	const uint8_t* bytes = (const uint8_t*)&slice.front();
	if (rem > 0) {
		uint8_t first = bytes[0];
		if (first < 0x80) { // 1 byte character
			out = static_cast<uint32_t>(first & 0x7F);
			slice.remove_prefix(1);
			return true;
		} else if (first < 0xC0) {
			// middle byte, not valid at start, fall through
		} else if (first < 0xE0) { // two byte character
			if (rem > 1) {
				uint8_t second = bytes[1];
				if (_IsSuffixChar(second)) {
					out = (static_cast<uint32_t>(first & 0x1F) << 6)
						+ static_cast<uint32_t>(second & 0x3F);
					slice.remove_prefix(2);
					return true;
				}
			}
		} else if (first < 0xF0) { // three byte character
			if (rem > 2) {
				uint8_t second = bytes[1];
				uint8_t third = bytes[2];
				if (_IsSuffixChar(second) && _IsSuffixChar(third)) {
					out = (static_cast<uint32_t>(first & 0x0F) << 12)
						+ (static_cast<uint32_t>(second & 0x3F) << 6)
						+ static_cast<uint32_t>(third & 0x3F);
					slice.remove_prefix(3);
					return true;
				}
			}
		} else if (first < 0xF8) { // four byte character
			if (rem > 3) {
				uint8_t second = bytes[1];
				uint8_t third = bytes[2];
				uint8_t fourth = bytes[3];
				if (_IsSuffixChar(second) && _IsSuffixChar(third) && _IsSuffixChar(fourth)) {
					out = (static_cast<uint32_t>(first & 0x07) << 18)
						+ (static_cast<uint32_t>(second & 0x3F) << 12)
						+ (static_cast<uint32_t>(third & 0x3F) << 6)
						+ static_cast<uint32_t>(fourth & 0x3F);
					slice.remove_prefix(4);
					return true;
				}
			}
		}
		out = static_cast<uint32_t>(REPLACEMENT_CHARACTER);
		slice.remove_prefix(1);
		return true;
	}
	return false;
}

/// See <https://www.ietf.org/rfc/rfc2781.html#section-2.1>
bool PushUTF16CodePoint(std::u16string& output, uint32_t U, size_t size) {
	if (output.length() >= size) return false;
	if (U < 0x10000) {
		// If U < 0x10000, encode U as a 16-bit unsigned integer and terminate.
		output.push_back(static_cast<uint16_t>(U));
		return true;
	} else if (U > 0x10FFFF) {
		output.push_back(REPLACEMENT_CHARACTER);
		return true;
	} else if (output.length() + 1 < size) {
		// Let U' = U - 0x10000. Because U is less than or equal to 0x10FFFF,
		// U' must be less than or equal to 0xFFFFF. That is, U' can be
		// represented in 20 bits.
		uint32_t Ut = U - 0x10000;

		// Initialize two 16-bit unsigned integers, W1 and W2, to 0xD800 and
		// 0xDC00, respectively. These integers each have 10 bits free to
		// encode the character value, for a total of 20 bits.
		uint16_t W1 = 0xD800;
		uint16_t W2 = 0xDC00;

		// Assign the 10 high-order bits of the 20-bit U' to the 10 low-order
		// bits of W1 and the 10 low-order bits of U' to the 10 low-order
		// bits of W2.
		W1 += static_cast<uint16_t>((Ut & 0x3FC00) >> 10);
		W2 += static_cast<uint16_t>((Ut & 0x3FF) >> 0);

		// Terminate.
		output.push_back(W1); // high surrogate
		output.push_back(W2); // low surrogate
		return true;
	} else return false;
}

std::u16string GeneralUtils::UTF8ToUTF16(const std::string_view& string, size_t size) {
	size_t newSize = MinSize(size, string);
	std::u16string output;
	output.reserve(newSize);
	std::string_view iterator = string;

	uint32_t c;
	while (_NextUTF8Char(iterator, c) && PushUTF16CodePoint(output, c, size)) {}
	return output;
}

//! Converts an std::string (ASCII) to UCS-2 / UTF-16
std::u16string GeneralUtils::ASCIIToUTF16(const std::string_view& string, size_t size) {
	size_t newSize = MinSize(size, string);
	std::u16string ret;
	ret.reserve(newSize);

	for (size_t i = 0; i < newSize; i++) {
		char c = string[i];
		// Note: both 7-bit ascii characters and REPLACEMENT_CHARACTER fit in one char16_t
		ret.push_back((c > 0 && c <= 127) ? static_cast<char16_t>(c) : REPLACEMENT_CHARACTER);
	}

	return ret;
}

//! Converts a (potentially-ill-formed) UTF-16 string to UTF-8
//! See: <http://simonsapin.github.io/wtf-8/#decoding-ill-formed-utf-16>
std::string GeneralUtils::UTF16ToWTF8(const std::u16string_view& string, size_t size) {
	size_t newSize = MinSize(size, string);
	std::string ret;
	ret.reserve(newSize);

	for (size_t i = 0; i < newSize; i++) {
		char16_t u = string[i];
		if (IsLeadSurrogate(u) && (i + 1) < newSize) {
			char16_t next = string[i + 1];
			if (IsTrailSurrogate(next)) {
				i += 1;
				char32_t cp = 0x10000
					+ ((static_cast<char32_t>(u) - 0xD800) << 10)
					+ (static_cast<char32_t>(next) - 0xDC00);
				PushUTF8CodePoint(ret, cp);
			} else {
				PushUTF8CodePoint(ret, u);
			}
		} else {
			PushUTF8CodePoint(ret, u);
		}
	}

	return ret;
}

bool GeneralUtils::CaseInsensitiveStringCompare(const std::string& a, const std::string& b) {
	return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) { return tolower(a) == tolower(b); });
}

// MARK: Bits

//! Sets a specific bit in a signed 64-bit integer
int64_t GeneralUtils::SetBit(int64_t value, uint32_t index) {
	return value |= 1ULL << index;
}

//! Clears a specific bit in a signed 64-bit integer
int64_t GeneralUtils::ClearBit(int64_t value, uint32_t index) {
	return value &= ~(1ULL << index);
}

//! Checks a specific bit in a signed 64-bit integer
bool GeneralUtils::CheckBit(int64_t value, uint32_t index) {
	return value & (1ULL << index);
}

bool GeneralUtils::ReplaceInString(std::string& str, const std::string& from, const std::string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

std::vector<std::wstring> GeneralUtils::SplitString(std::wstring& str, wchar_t delimiter) {
	std::vector<std::wstring> vector = std::vector<std::wstring>();
	std::wstring current;

	for (const auto& c : str) {
		if (c == delimiter) {
			vector.push_back(current);
			current = L"";
		} else {
			current += c;
		}
	}

	vector.push_back(current);
	return vector;
}

std::vector<std::u16string> GeneralUtils::SplitString(std::u16string& str, char16_t delimiter) {
	std::vector<std::u16string> vector = std::vector<std::u16string>();
	std::u16string current;

	for (const auto& c : str) {
		if (c == delimiter) {
			vector.push_back(current);
			current = u"";
		} else {
			current += c;
		}
	}

	vector.push_back(current);
	return vector;
}

std::vector<std::string> GeneralUtils::SplitString(const std::string& str, char delimiter) {
	std::vector<std::string> vector = std::vector<std::string>();
	std::string current = "";

	for (size_t i = 0; i < str.length(); i++) {
		char c = str[i];

		if (c == delimiter) {
			vector.push_back(current);
			current = "";
		} else {
			current += c;
		}
	}

	vector.push_back(current);

	return vector;
}

std::u16string GeneralUtils::ReadWString(RakNet::BitStream* inStream) {
	uint32_t length;
	inStream->Read<uint32_t>(length);

	std::u16string string;
	for (auto i = 0; i < length; i++) {
		uint16_t c;
		inStream->Read(c);
		string.push_back(c);
	}

	return string;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

std::vector<std::string> GeneralUtils::GetFileNamesFromFolder(const std::string& folder) {
	std::vector<std::string> names;
	std::string search_path = folder + "/*.*";
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return names;
}
#else
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <iostream>
#include <vector>
#include <cstring>

std::vector<std::string> GeneralUtils::GetFileNamesFromFolder(const std::string& folder) {
	std::vector<std::string> names;
	struct dirent* entry;
	DIR* dir = opendir(folder.c_str());
	if (dir == NULL) {
		return names;
	}

	while ((entry = readdir(dir)) != NULL) {
		std::string value(entry->d_name, strlen(entry->d_name));
		if (value == "." || value == "..") {
			continue;
		}
		names.push_back(value);
	}

	closedir(dir);

	return names;
}
#endif
