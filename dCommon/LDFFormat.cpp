#include "LDFFormat.h"

// Custom Classes
#include "GeneralUtils.h"

#include "Game.h"
#include "Logger.h"

// C++
#include <string_view>
#include <vector>

using LDFKey = std::string_view;
using LDFTypeAndValue = std::string_view;

using LDFType = std::string_view;
using LDFValue = std::string_view;

//! Returns a pointer to a LDFData value based on string format
LDFBaseData* LDFBaseData::DataFromString(const std::string_view& format) {
	// A valid LDF must be at least 3 characters long (=0:) is the shortest valid LDF (empty UTF-16 key with no initial value)
	if (format.empty() || format.length() <= 2) return nullptr;
	auto equalsPosition = format.find('=');
	// You can have an empty key, just make sure the type and value might exist
	if (equalsPosition == std::string::npos || equalsPosition == (format.size() - 1)) return nullptr;

	std::pair<LDFKey, LDFTypeAndValue> keyValue;
	keyValue.first = format.substr(0, equalsPosition);
	keyValue.second = format.substr(equalsPosition + 1, format.size());

	std::u16string key = GeneralUtils::ASCIIToUTF16(keyValue.first);

	auto colonPosition = keyValue.second.find(':');

	// If : is the first thing after an =, then this is an invalid LDF since
	// we dont have a type to use.
	if (colonPosition == std::string::npos || colonPosition == 0) return nullptr;

	std::pair<LDFType, LDFValue> ldfTypeAndValue;
	ldfTypeAndValue.first = keyValue.second.substr(0, colonPosition);
	ldfTypeAndValue.second = keyValue.second.substr(colonPosition + 1, keyValue.second.size());

	// Only allow empty values for string values.
	if (ldfTypeAndValue.second.size() == 0 && !(ldfTypeAndValue.first == "0" || ldfTypeAndValue.first == "13")) return nullptr;

	eLDFType type;
	char* storage;
	try {
		type = static_cast<eLDFType>(strtol(ldfTypeAndValue.first.data(), &storage, 10));
	} catch (std::exception) {
		Log("Attempted to process invalid ldf type (%s) from string (%s)", ldfTypeAndValue.first.data(), format.data());
		return nullptr;
	}

	LDFBaseData* returnValue = nullptr;
	switch (type) {
	case LDF_TYPE_UTF_16: {
		std::u16string data = GeneralUtils::UTF8ToUTF16(ldfTypeAndValue.second);
		returnValue = new LDFData<std::u16string>(key, data);
		break;
	}

	case LDF_TYPE_S32: {
		try {
			int32_t data = static_cast<int32_t>(strtoul(ldfTypeAndValue.second.data(), &storage, 10));
			returnValue = new LDFData<int32_t>(key, data);
		} catch (std::exception) {
			LogWarning("Attempted to process invalid int32 value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_FLOAT: {
		try {
			float data = strtof(ldfTypeAndValue.second.data(), &storage);
			returnValue = new LDFData<float>(key, data);
		} catch (std::exception) {
			LogWarning("Attempted to process invalid float value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_DOUBLE: {
		try {
			double data = strtod(ldfTypeAndValue.second.data(), &storage);
			returnValue = new LDFData<double>(key, data);
		} catch (std::exception) {
			LogWarning("Attempted to process invalid double value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_U32:
	{
		uint32_t data;

		if (ldfTypeAndValue.second == "true") {
			data = 1;
		} else if (ldfTypeAndValue.second == "false") {
			data = 0;
		} else {
			try {
				data = static_cast<uint32_t>(strtoul(ldfTypeAndValue.second.data(), &storage, 10));
			} catch (std::exception) {
				LogWarning("Attempted to process invalid uint32 value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
				return nullptr;
			}
		}

		returnValue = new LDFData<uint32_t>(key, data);
		break;
	}

	case LDF_TYPE_BOOLEAN: {
		bool data;

		if (ldfTypeAndValue.second == "true") {
			data = true;
		} else if (ldfTypeAndValue.second == "false") {
			data = false;
		} else {
			try {
				data = static_cast<bool>(strtol(ldfTypeAndValue.second.data(), &storage, 10));
			} catch (std::exception) {
				LogWarning("Attempted to process invalid bool value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
				return nullptr;
			}
		}

		returnValue = new LDFData<bool>(key, data);
		break;
	}

	case LDF_TYPE_U64: {
		try {
			uint64_t data = static_cast<uint64_t>(strtoull(ldfTypeAndValue.second.data(), &storage, 10));
			returnValue = new LDFData<uint64_t>(key, data);
		} catch (std::exception) {
			LogWarning("Attempted to process invalid uint64 value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_OBJID: {
		try {
			LWOOBJID data = static_cast<LWOOBJID>(strtoll(ldfTypeAndValue.second.data(), &storage, 10));
			returnValue = new LDFData<LWOOBJID>(key, data);
		} catch (std::exception) {
			LogWarning("Attempted to process invalid LWOOBJID value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_UTF_8: {
		std::string data = ldfTypeAndValue.second.data();
		returnValue = new LDFData<std::string>(key, data);
		break;
	}

	case LDF_TYPE_UNKNOWN: {
		LogWarning("Attempted to process invalid unknown value (%s) from string (%s)", ldfTypeAndValue.second.data(), format.data());
		break;
	}

	default: {
		LogWarning("Attempted to process invalid LDF type (%d) from string (%s)", type, format.data());
		break;
	}
	}
	return returnValue;
}
