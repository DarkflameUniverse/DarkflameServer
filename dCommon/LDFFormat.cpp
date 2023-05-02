#include "LDFFormat.h"

// Custom Classes
#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"

// C++
#include <sstream>
#include <vector>

using LDFKey = std::string;
using LDFTypeAndValue = std::string;

using LDFType = std::string;
using LDFValue = std::string;

//! Returns a pointer to a LDFData value based on string format
LDFBaseData* LDFBaseData::DataFromString(const std::string& format) {

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
	try {
		type = static_cast<eLDFType>(stoi(ldfTypeAndValue.first));
	} catch (std::exception) {
		Game::logger->Log("LDFFormat", "Attempted to process invalid ldf type (%s) from string (%s)", ldfTypeAndValue.first, format.c_str());
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
			int32_t data = static_cast<int32_t>(std::stoul(ldfTypeAndValue.second));
			returnValue = new LDFData<int32_t>(key, data);
		} catch (std::exception) {
			Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid int32 value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_FLOAT: {
		try {
			float data = std::stof(ldfTypeAndValue.second);
			returnValue = new LDFData<float>(key, data);
		} catch (std::exception) {
			Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid float value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_DOUBLE: {
		try {
			double data = std::stod(ldfTypeAndValue.second);
			returnValue = new LDFData<double>(key, data);
		} catch (std::exception) {
			Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid double value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
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
				data = static_cast<uint32_t>(std::stoul(ldfTypeAndValue.second));
			} catch (std::exception) {
				Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid uint32 value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
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
				data = static_cast<bool>(std::stoi(ldfTypeAndValue.second));
			} catch (std::exception) {
				Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid bool value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
				return nullptr;
			}
		}

		returnValue = new LDFData<bool>(key, data);
		break;
	}

	case LDF_TYPE_U64: {
		try {
			uint64_t data = static_cast<uint64_t>(std::stoull(ldfTypeAndValue.second));
			returnValue = new LDFData<uint64_t>(key, data);
		} catch (std::exception) {
			Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid uint64 value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_OBJID: {
		try {
			LWOOBJID data = static_cast<LWOOBJID>(std::stoll(ldfTypeAndValue.second));
			returnValue = new LDFData<LWOOBJID>(key, data);
		} catch (std::exception) {
			Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid LWOOBJID value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
			return nullptr;
		}
		break;
	}

	case LDF_TYPE_UTF_8: {
		std::string data = ldfTypeAndValue.second;
		returnValue = new LDFData<std::string>(key, data);
		break;
	}

	case LDF_TYPE_UNKNOWN: {
		Game::logger->Log("LDFFormat", "Warning: Attempted to process invalid unknown value (%s) from string (%s)", ldfTypeAndValue.second, format.c_str());
		break;
	}
	}

	return returnValue;
}
