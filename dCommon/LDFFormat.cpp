#include "LDFFormat.h"

// Custom Classes
#include "GeneralUtils.h"

#include "Game.h"
#include "Logger.h"

// C++
#include <string_view>
#include <vector>

//! Returns a pointer to a LDFData value based on string format
std::unique_ptr<LDFBaseData> LDFBaseData::DataFromString(const std::string_view& format) {
	std::unique_ptr<LDFBaseData> toReturn;
	// A valid LDF must be at least 3 characters long (=0:) is the shortest valid LDF (empty UTF-16 key with no initial value)
	if (!format.empty() && format.length() > 2) {
		auto equalsPosition = format.find('=');
		// You can have an empty key, just make sure the type and value might exist
		if (equalsPosition != std::string::npos && equalsPosition != (format.size() - 1)) {

			const std::string_view keyValue = format.substr(0, equalsPosition);
			const std::string_view typeAndValue = format.substr(equalsPosition + 1, format.size());

			const auto key = GeneralUtils::ASCIIToUTF16(keyValue);

			const auto colonPosition = typeAndValue.find(':');

			// If : is the first thing after an =, then this is an invalid LDF since
			// we dont have a type to use.
			if (colonPosition != std::string::npos && colonPosition != 0) {
				const std::string_view ldfType = typeAndValue.substr(0, colonPosition);
				const std::string_view ldfValue = typeAndValue.substr(colonPosition + 1, typeAndValue.size());

				// Only allow empty values for string values.
				if (!ldfValue.empty() || (ldfType == "0" /* UTF-16 */ || ldfType == "13" /* UTF-8 */)) {
					const eLDFType type = GeneralUtils::TryParse<eLDFType>(ldfType, LDF_TYPE_UNKNOWN);
					switch (type) {
					case LDF_TYPE_UTF_16: {
						std::u16string data = GeneralUtils::UTF8ToUTF16(ldfValue);
						toReturn.reset(new LDFData<std::u16string>(key, data));
						break;
					}

					case LDF_TYPE_S32: {
						const auto data = GeneralUtils::TryParse<int32_t>(ldfValue);
						if (data) {
							toReturn.reset(new LDFData<int32_t>(key, data.value()));
						} else {
							LOG("Warning: Attempted to process invalid int32 value (%s) from string (%s)", ldfValue.data(), format.data());
						}

						break;
					}

					case LDF_TYPE_FLOAT: {
						const auto data = GeneralUtils::TryParse<float>(ldfValue);
						if (data) {
							toReturn.reset(new LDFData<float>(key, data.value()));
						} else {
							LOG("Warning: Attempted to process invalid float value (%s) from string (%s)", ldfValue.data(), format.data());
						}
						break;
					}

					case LDF_TYPE_DOUBLE: {
						const auto data = GeneralUtils::TryParse<double>(ldfValue);
						if (data) {
							toReturn.reset(new LDFData<double>(key, data.value()));
						} else {
							LOG("Warning: Attempted to process invalid double value (%s) from string (%s)", ldfValue.data(), format.data());
						}
						break;
					}

					case LDF_TYPE_U32:
					{
						uint32_t data;
						bool parsed = true;
						// Have to do this really weird parsing to allow for copy ellision
						if (ldfValue == "true") {
							data = 1;
						} else if (ldfValue == "false") {
							data = 0;
						} else {
							const auto dataOptional = GeneralUtils::TryParse<uint32_t>(ldfValue);
							if (!dataOptional) {
								LOG("Warning: Attempted to process invalid uint32 value (%s) from string (%s)", ldfValue.data(), format.data());
								parsed = false;
							} else {
								data = dataOptional.value();
							}
						}

						if (parsed) toReturn.reset(new LDFData<uint32_t>(key, data));
						break;
					}

					case LDF_TYPE_BOOLEAN: {
						bool data;
						bool parsed = true;
						// Have to do this really weird parsing to allow for copy ellision
						if (ldfValue == "true") {
							data = true;
						} else if (ldfValue == "false") {
							data = false;
						} else {
							const auto dataOptional = GeneralUtils::TryParse<bool>(ldfValue);
							if (!dataOptional) {
								LOG("Warning: Attempted to process invalid bool value (%s) from string (%s)", ldfValue.data(), format.data());
								parsed = false;
							} else {
								data = dataOptional.value();
							}
						}

						if (parsed) toReturn.reset(new LDFData<bool>(key, data));
						break;
					}

					case LDF_TYPE_U64: {
						const auto data = GeneralUtils::TryParse<uint64_t>(ldfValue);
						if (data) {
							toReturn.reset(new LDFData<uint64_t>(key, data.value()));
						} else {
							LOG("Warning: Attempted to process invalid uint64 value (%s) from string (%s)", ldfValue.data(), format.data());
						}
						break;
					}

					case LDF_TYPE_OBJID: {
						const auto data = GeneralUtils::TryParse<LWOOBJID>(ldfValue);
						if (data) {
							toReturn.reset(new LDFData<LWOOBJID>(key, data.value()));
						} else {
							LOG("Warning: Attempted to process invalid LWOOBJID value (%s) from string (%s)", ldfValue.data(), format.data());
						}
						break;
					}

					case LDF_TYPE_UTF_8: {
						toReturn.reset(new LDFData<std::string>(key, ldfValue.data()));
						break;
					}

					case LDF_TYPE_UNKNOWN:
						[[fallthrough]];
					default: {
						LOG("Warning: Attempted to process invalid unknown value (%s) from string (%s)", ldfValue.data(), format.data());
						break;
					}

					}
				}
			}
		}
	}

	return toReturn;
}
