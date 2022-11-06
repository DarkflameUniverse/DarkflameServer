#include "LDFFormat.h"

// Custom Classes
#include "GeneralUtils.h"

// C++
#include <sstream>
#include <vector>

//! Returns a pointer to a LDFData value based on string format
LDFBaseData* LDFBaseData::DataFromString(const std::string& format) {

	// First, check the format
	std::istringstream ssFormat(format);
	std::string token;

	std::vector<std::string> keyValueArray;
	while (std::getline(ssFormat, token, '=')) {
		keyValueArray.push_back(token);
	}

	if (keyValueArray.size() == 2) {
		std::u16string key = GeneralUtils::ASCIIToUTF16(keyValueArray[0]);

		std::vector<std::string> dataArray;
		std::istringstream ssData(keyValueArray[1]);
		while (std::getline(ssData, token, ':')) {
			dataArray.push_back(token);
		}

		if (dataArray.size() > 2) { // hacky fix for strings with colons in them
			std::vector<std::string> newDataArray;
			newDataArray.push_back(dataArray[0]);
			std::string value = "";
			for (size_t i = 1; i < dataArray.size(); ++i) {
				value += dataArray[i] + ':';
			}
			value.pop_back(); // remove last colon
			newDataArray.push_back(value);
			dataArray = newDataArray;
		}

		if ((dataArray[0] == "0" || dataArray[0] == "13") && dataArray.size() == 1) {
			dataArray.push_back("");
		}

		if (dataArray.size() == 2) {
			eLDFType type = static_cast<eLDFType>(stoi(dataArray[0]));

			switch (type) {
			case LDF_TYPE_UTF_16: {
				std::u16string data = GeneralUtils::UTF8ToUTF16(dataArray[1]);
				return new LDFData<std::u16string>(key, data);
			}

			case LDF_TYPE_S32: {
				int32_t data = static_cast<int32_t>(stoull(dataArray[1]));
				return new LDFData<int32_t>(key, data);
			}

			case LDF_TYPE_FLOAT: {
				float data = static_cast<float>(stof(dataArray[1]));
				return new LDFData<float>(key, data);
			}

			case LDF_TYPE_DOUBLE: {
				double data = static_cast<float>(stod(dataArray[1]));
				return new LDFData<double>(key, data);
			}

			case LDF_TYPE_U32:
			{
				uint32_t data;

				if (dataArray[1] == "true") {
					data = 1;
				} else if (dataArray[1] == "false") {
					data = 0;
				} else {
					data = static_cast<uint32_t>(stoul(dataArray[1]));
				}

				return new LDFData<uint32_t>(key, data);
			}

			case LDF_TYPE_BOOLEAN: {
				bool data;

				if (dataArray[1] == "true") {
					data = true;
				} else if (dataArray[1] == "false") {
					data = false;
				} else {
					data = static_cast<bool>(stoi(dataArray[1]));
				}

				return new LDFData<bool>(key, data);
			}

			case LDF_TYPE_U64: {
				uint64_t data = static_cast<uint64_t>(stoull(dataArray[1]));
				return new LDFData<uint64_t>(key, data);
			}

			case LDF_TYPE_OBJID: {
				LWOOBJID data = static_cast<LWOOBJID>(stoll(dataArray[1]));
				return new LDFData<LWOOBJID>(key, data);
			}

			case LDF_TYPE_UTF_8: {
				std::string data = dataArray[1];
				return new LDFData<std::string>(key, data);
			}

			case LDF_TYPE_UNKNOWN: {
				return nullptr;
			}
			}
		}
	}

	return nullptr;

}
