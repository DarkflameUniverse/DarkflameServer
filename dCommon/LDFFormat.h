#ifndef LDFFORMAT_H
#define LDFFORMAT_H

// Custom Classes
#include "dCommonVars.h"
#include "GeneralUtils.h"

// C++
#include <map>
#include <string>
#include <string_view>
#include <sstream>

// RakNet
#include "BitStream.h"

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

class LDFBaseData {
public:

	virtual ~LDFBaseData() {}

	virtual void WriteToPacket(RakNet::BitStream& packet) const = 0;

	virtual const std::u16string& GetKey() const = 0;

	virtual eLDFType GetValueType() const = 0;

	/** Gets a string from the key/value pair
	 * @param includeKey Whether or not to include the key in the data
	 * @param includeTypeId Whether or not to include the type id in the data
	 * @return The string representation of the data
	 */
	virtual std::string GetString(bool includeKey = true, bool includeTypeId = true) const = 0;

	virtual std::string GetValueAsString() const = 0;

	virtual std::unique_ptr<LDFBaseData> Copy() const = 0;

	/**
	 * Given an input string, return the data as a LDF key.
	 */
	static std::unique_ptr<LDFBaseData> DataFromString(const std::string_view& format);

};

template<typename T>
class LDFData : public LDFBaseData {
private:
	std::u16string key;
	T value;

	//! Writes the key to the packet
	void WriteKey(RakNet::BitStream& packet) const {
		packet.Write<uint8_t>(this->key.length() * sizeof(uint16_t));
		for (uint32_t i = 0; i < this->key.length(); ++i) {
			packet.Write<uint16_t>(this->key[i]);
		}
	}

	//! Writes the value to the packet
	void WriteValue(RakNet::BitStream& packet) const {
		packet.Write<uint8_t>(this->GetValueType());
		packet.Write(this->value);
	}

public:

	//! Initializer
	LDFData(const std::u16string& key, const T& value) {
		this->key = key;
		this->value = value;
	}

	//! Initializer
	LDFData(const std::string& key, const T& value) {
		this->key = GeneralUtils::ASCIIToUTF16(key);
		this->value = value;
	}

	//! Destructor
	~LDFData(void) override {}

	//! Gets the value
	/*!
	  \return The value
	 */
	const T& GetValue(void) const { return this->value; }

	//! Sets the value
	/*!
	  \param value The value to set to
	 */
	void SetValue(T value) { this->value = value; };

	//! Gets the value string
	/*!
	  \return The value string
	 */
	std::string GetValueString(void) const { return ""; }

	//! Writes the data to a packet
	/*!
	  \param packet The packet
	 */
	void WriteToPacket(RakNet::BitStream& packet) const override {
		this->WriteKey(packet);
		this->WriteValue(packet);
	}

	//! Gets the key
	/*!
	 \return The key
	 */
	const std::u16string& GetKey(void) const override { return this->key; }

	//! Gets the LDF Type
	/*!
	 \return The LDF value type
	 */
	eLDFType GetValueType(void) const override { return LDF_TYPE_UNKNOWN; }

	//! Gets the string data
	/*!
	  \param includeKey Whether or not to include the key in the data
	  \param includeTypeId Whether or not to include the type id in the data
	  \return The string representation of the data
	 */
	std::string GetString(const bool includeKey = true, const bool includeTypeId = true) const override {
		if (GetValueType() == -1) {
			return GeneralUtils::UTF16ToWTF8(this->key) + "=-1:<server variable>";
		}

		std::stringstream stream;

		if (includeKey) {
			const std::string& sKey = GeneralUtils::UTF16ToWTF8(this->key, this->key.size());
			stream << sKey << '=';
		}

		if (includeTypeId) {
			stream << this->GetValueType() << ':';
		}

		const std::string& sData = this->GetValueString();

		stream << sData;

		return stream.str();
	}

	std::string GetValueAsString() const override {
		return this->GetValueString();
	}

	std::unique_ptr<LDFBaseData> Copy() const override {
		return std::make_unique<LDFData<T>>(key, value);
	}

	inline static const T Default = {};
};

// LDF Types
template<> inline eLDFType LDFData<std::u16string>::GetValueType(void) const { return LDF_TYPE_UTF_16; };
template<> inline eLDFType LDFData<int32_t>::GetValueType(void) const { return LDF_TYPE_S32; };
template<> inline eLDFType LDFData<float>::GetValueType(void) const { return LDF_TYPE_FLOAT; };
template<> inline eLDFType LDFData<double>::GetValueType(void) const { return LDF_TYPE_DOUBLE; };
template<> inline eLDFType LDFData<uint32_t>::GetValueType(void) const { return LDF_TYPE_U32; };
template<> inline eLDFType LDFData<bool>::GetValueType(void) const { return LDF_TYPE_BOOLEAN; };
template<> inline eLDFType LDFData<uint64_t>::GetValueType(void) const { return LDF_TYPE_U64; };
template<> inline eLDFType LDFData<LWOOBJID>::GetValueType(void) const { return LDF_TYPE_OBJID; };
template<> inline eLDFType LDFData<std::string>::GetValueType(void) const { return LDF_TYPE_UTF_8; };

// The specialized version for std::u16string (UTF-16)
template<>
inline void LDFData<std::u16string>::WriteValue(RakNet::BitStream& packet) const {
	packet.Write<uint8_t>(this->GetValueType());

	packet.Write<uint32_t>(this->value.length());
	for (uint32_t i = 0; i < this->value.length(); ++i) {
		packet.Write<uint16_t>(this->value[i]);
	}
}

// The specialized version for bool
template<>
inline void LDFData<bool>::WriteValue(RakNet::BitStream& packet) const {
	packet.Write<uint8_t>(this->GetValueType());

	packet.Write<uint8_t>(this->value);
}

// The specialized version for std::string (UTF-8)
template<>
inline void LDFData<std::string>::WriteValue(RakNet::BitStream& packet) const {
	packet.Write<uint8_t>(this->GetValueType());

	packet.Write<uint32_t>(this->value.length());
	for (uint32_t i = 0; i < this->value.length(); ++i) {
		packet.Write<uint8_t>(this->value[i]);
	}
}

template<> inline std::string LDFData<std::u16string>::GetValueString() const {
	return GeneralUtils::UTF16ToWTF8(this->value, this->value.size());
}

template<> inline std::string LDFData<int32_t>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<float>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<double>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<uint32_t>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<bool>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<uint64_t>::GetValueString() const { return std::to_string(this->value); }
template<> inline std::string LDFData<LWOOBJID>::GetValueString() const { return std::to_string(this->value); }

template<> inline std::string LDFData<std::string>::GetValueString() const { return this->value; }

struct LwoNameValue {
	using LDFPtr = std::unique_ptr<LDFBaseData>;
	using ValueType = std::map<std::u16string, LDFPtr>;

	LwoNameValue& operator=(const LwoNameValue& other) {
		this->values = other.Copy();
		return *this;
	}

	template<typename T>
	void Insert(const std::u16string& key, const T& value) {
		this->values.insert_or_assign(key, std::unique_ptr(std::make_unique<LDFData<T>>(key, value)));
	}

	void Insert(const std::u16string& key, const char* value) {
		this->Insert<std::string>(key, value);
	}

	void Insert(const std::u16string& key, const char16_t* value) {
		this->Insert<std::u16string>(key, value);
	}

	template<typename T>
	void Insert(const std::string& key, const T& value) {
		this->Insert<T>(GeneralUtils::UTF8ToUTF16(key), value);
	}

	void Insert(const std::string& key, const char* value) {
		this->Insert<std::string>(GeneralUtils::UTF8ToUTF16(key), value);
	}

	void Insert(const std::string& key, const char16_t* value) {
		this->Insert<std::u16string>(GeneralUtils::UTF8ToUTF16(key), value);
	}

	const LDFPtr& ParseInsert(const std::string& data) {
		LDFPtr toInsert(LDFBaseData::DataFromString(data));
		return toInsert ?
		this->values.insert_or_assign(toInsert->GetKey(), std::move(toInsert)).first->second :
		this->values.insert_or_assign(u"FAILED_TO_PARSE_" + GeneralUtils::UTF8ToUTF16(data), std::make_unique<LDFData<std::string>>("", "")).first->second;
	}

	const LDFPtr& ParseInsert(const std::u16string& data) {
		return this->ParseInsert(GeneralUtils::UTF16ToWTF8(data));
	}

	ValueType::const_iterator begin() const {
		return this->values.cbegin();
	}

	ValueType::const_iterator end() const {
		return this->values.cend();
	}

	void Erase(const std::u16string& key) {
		this->values.erase(key);
	}

	void Erase(const std::string& key) {
		this->Erase(GeneralUtils::ASCIIToUTF16(key));
	}

	LwoNameValue() = default;

	LwoNameValue(const LwoNameValue& other) {
		this->values = other.Copy();
	}

	ValueType values;
private:
	ValueType Copy() const {
		ValueType copy;
		for (const auto& [key, value] : this->values) copy.insert_or_assign(key, value->Copy());
		return copy;
	}
};

#endif  //!LDFFORMAT_H
