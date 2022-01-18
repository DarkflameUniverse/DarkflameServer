#include "GeneralUtils.h"

// C++
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <locale>
#include <codecvt>

template <typename T>
inline size_t MinSize(size_t size, const std::basic_string<T>& string) {
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
        ret.push_back(cp);
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

//! Converts an std::string (ASCII) to UCS-2 / UTF-16
std::u16string GeneralUtils::ASCIIToUTF16(const std::string& string, size_t size) {
    size_t newSize = MinSize(size, string);
    std::u16string ret;
    ret.reserve(newSize);

    for (size_t i = 0; i < newSize; i++) {
        char c = string[i];
        assert(c > 0 && c <= 127);
        ret.push_back(static_cast<char16_t>(c));
    }

    return ret;
}

//! Converts an UCS-2 / UTF-16 to std::string (ASCII)
std::string GeneralUtils::UTF16ToWTF8(const std::u16string& string, size_t size) {
    std::wstring_convert<std::codecvt<char, char16_t, std::mbstate_t>, char16_t> convert;
    return convert.to_bytes(string.substr(0, size));
}

bool GeneralUtils::CaseInsensitiveStringCompare(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end (), b.begin(), b.end(),[](char a, char b) { return tolower(a) == tolower(b); });
}

std::u16string GeneralUtils::UTF8ToUTF16(const std::string& string, size_t size) {
    std::wstring_convert<std::codecvt<char, char16_t, std::mbstate_t>, char16_t> convert;
    return convert.from_bytes(string.substr(0, size));
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
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

std::vector<std::wstring> GeneralUtils::SplitString(std::wstring& str, wchar_t delimiter)
{
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

std::vector<std::u16string> GeneralUtils::SplitString(std::u16string& str, char16_t delimiter)
{
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

std::vector<std::string> GeneralUtils::SplitString(const std::string& str, char delimiter)
{
	std::vector<std::string> vector = std::vector<std::string>();
	std::string current = "";

	for (size_t i = 0; i < str.length(); i++)
	{
		char c = str[i];

		if (c == delimiter)
		{
			vector.push_back(current);
			current = "";
		}
		else
		{
			current += c;
		}
	}

	vector.push_back(current);

	return vector;
}

std::u16string GeneralUtils::ReadWString(RakNet::BitStream *inStream) {
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
