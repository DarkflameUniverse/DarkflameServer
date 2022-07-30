#pragma once

// Custom Classes
#include "dCommonVars.h"
#include "GeneralUtils.h"

// C++
#include <string>
#include <sstream>

// RakNet

#include "../thirdparty/raknet/Source/BitStream.h"

/*!
  \file LDFFormat.hpp
  \brief A collection of LDF format classes
 */

 //! An enum for LDF Data Types
enum eLDFType {
	LDF_TYPE_UNKNOWN = -1,          //!< Unknown data type
	LDF_TYPE_UTF_16 = 0,            //!< UTF-16 wstring data type
	LDF_TYPE_S32 = 1,               //!< Signed 32-bit data type
	LDF_TYPE_FLOAT = 3,             //!< Float data type
	LDF_TYPE_DOUBLE = 4,            //!< Double data type
	LDF_TYPE_U32 = 5,               //!< Unsigned 32-bit data type
	LDF_TYPE_BOOLEAN = 7,           //!< Boolean data type
	LDF_TYPE_U64 = 8,               //!< Unsigned 64-bit data type (originally signed, templates won't work with both S64 & OBJID
	LDF_TYPE_OBJID = 9,             //!< Signed 64-bit data type (reserved for object IDs)
	LDF_TYPE_UTF_8 = 13,            //!< UTF-8 string data type
};

//! A base class for the LDF data
class LDFBaseData {
public:

	//! Destructor
	virtual ~LDFBaseData(void) {}

	//! Writes the data to a packet
	/*!
	  \param packet The packet
	 */
	virtual void WriteToPacket(RakNet::BitStream* packet) = 0;

	//! Gets the key
	/*!
	  \return The key
	 */
	virtual const std::u16string& GetKey(void) = 0;

	//! Gets the value type
	/*!
	  \return The value type
	 */
	virtual eLDFType GetValueType(void) = 0;

	//! Gets a string from the key/value pair
	/*!
	  \param includeKey Whether or not to include the key in the data
	  \param includeTypeId Whether or not to include the type id in the data
	  \return The string representation of the data
	 */
	virtual std::string GetString(bool includeKey = true, bool includeTypeId = true) = 0;

	virtual std::string GetValueAsString() = 0;

	virtual LDFBaseData* Copy() = 0;

	// MARK: Functions

	//! Returns a pointer to a LDFData value based on string format
	/*!
	  \param format The format
	 */
	static LDFBaseData* DataFromString(const std::string& format);

};

//! A structure for an LDF key-value pair
template<typename T>
class LDFData : public LDFBaseData {
private:
	std::u16string key;
	T value;

	//! Writes the key to the packet
	void WriteKey(RakNet::BitStream* packet) {
		packet->Write(static_cast<uint8_t>(this->key.length() * sizeof(uint16_t)));
		for (uint32_t i = 0; i < this->key.length(); ++i) {
			packet->Write(static_cast<uint16_t>(this->key[i]));
		}
	}

	//! Writes the value to the packet
	void WriteValue(RakNet::BitStream* packet) {
		packet->Write(static_cast<uint8_t>(this->GetValueType()));
		packet->Write(this->value);
	}

public:

	//! Initializer
	LDFData(const std::u16string& key, const T& value) {
		this->key = key;
		this->value = value;
	}

	//! Destructor
	~LDFData(void) override {}

	//! Gets the value
	/*!
	  \return The value
	 */
	const T& GetValue(void) { return this->value; }

	//! Sets the value
	/*!
	  \param value The value to set to
	 */
	void SetValue(T value) { this->value = value; };

	//! Gets the value string
	/*!
	  \return The value string
	 */
	std::string GetValueString(void) { return ""; }

	//! Writes the data to a packet
	/*!
	  \param packet The packet
	 */
	void WriteToPacket(RakNet::BitStream* packet) override {
		this->WriteKey(packet);
		this->WriteValue(packet);
	}

	//! Gets the key
	/*!
	 \return The key
	 */
	const std::u16string& GetKey(void) override { return this->key; }

	//! Gets the LDF Type
	/*!
	 \return The LDF value type
	 */
	eLDFType GetValueType(void) override { return LDF_TYPE_UNKNOWN; }

	//! Gets the string data
	/*!
	  \param includeKey Whether or not to include the key in the data
	  \param includeTypeId Whether or not to include the type id in the data
	  \return The string representation of the data
	 */
	std::string GetString(const bool includeKey = true, const bool includeTypeId = true) override {
		if (GetValueType() == -1) {
			return GeneralUtils::UTF16ToWTF8(this->key) + "=-1:<server variable>";
		}

		std::stringstream stream;

		if (includeKey) {
			const std::string& sKey = GeneralUtils::UTF16ToWTF8(this->key, this->key.size());

			stream << sKey << "=";
		}

		if (includeTypeId) {
			const std::string& sType = std::to_string(this->GetValueType());


			stream << sType << ":";
		}

		const std::string& sData = this->GetValueString();

		stream << sData;

		return stream.str();
	}

	std::string GetValueAsString() override {
		return this->GetValueString();
	}

	LDFBaseData* Copy() override {
		return new LDFData<T>(key, value);
	}

	inline static T Default = {};
};

// LDF Types
template<> inline eLDFType LDFData<std::u16string>::GetValueType(void) { return LDF_TYPE_UTF_16; };
template<> inline eLDFType LDFData<int32_t>::GetValueType(void) { return LDF_TYPE_S32; };
template<> inline eLDFType LDFData<float>::GetValueType(void) { return LDF_TYPE_FLOAT; };
template<> inline eLDFType LDFData<double>::GetValueType(void) { return LDF_TYPE_DOUBLE; };
template<> inline eLDFType LDFData<uint32_t>::GetValueType(void) { return LDF_TYPE_U32; };
template<> inline eLDFType LDFData<bool>::GetValueType(void) { return LDF_TYPE_BOOLEAN; };
template<> inline eLDFType LDFData<uint64_t>::GetValueType(void) { return LDF_TYPE_U64; };
template<> inline eLDFType LDFData<LWOOBJID>::GetValueType(void) { return LDF_TYPE_OBJID; };
template<> inline eLDFType LDFData<std::string>::GetValueType(void) { return LDF_TYPE_UTF_8; };

// The specialized version for std::u16string (UTF-16)
template<>
inline void LDFData<std::u16string>::WriteValue(RakNet::BitStream* packet) {
	packet->Write(static_cast<uint8_t>(this->GetValueType()));

	packet->Write(static_cast<uint32_t>(this->value.length()));
	for (uint32_t i = 0; i < this->value.length(); ++i) {
		packet->Write(static_cast<uint16_t>(this->value[i]));
	}
}

// The specialized version for bool
template<>
inline void LDFData<bool>::WriteValue(RakNet::BitStream* packet) {
	packet->Write(static_cast<uint8_t>(this->GetValueType()));

	packet->Write(static_cast<uint8_t>(this->value));
}

// The specialized version for std::string (UTF-8)
template<>
inline void LDFData<std::string>::WriteValue(RakNet::BitStream* packet) {
	packet->Write(static_cast<uint8_t>(this->GetValueType()));

	packet->Write(static_cast<uint32_t>(this->value.length()));
	for (uint32_t i = 0; i < this->value.length(); ++i) {
		packet->Write(static_cast<uint8_t>(this->value[i]));
	}
}

// MARK: String Data
template<> inline std::string LDFData<std::u16string>::GetValueString(void) {
	//std::string toReturn(this->value.begin(), this->value.end());
	//return toReturn;

	return GeneralUtils::UTF16ToWTF8(this->value, this->value.size());
}

template<> inline std::string LDFData<int32_t>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<float>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<double>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<uint32_t>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<bool>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<uint64_t>::GetValueString(void) { return std::to_string(this->value); }
template<> inline std::string LDFData<LWOOBJID>::GetValueString(void) { return std::to_string(this->value); }

template<> inline std::string LDFData<std::string>::GetValueString(void) { return this->value; }
