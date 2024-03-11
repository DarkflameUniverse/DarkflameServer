#ifndef __AMF3__H__
#define __AMF3__H__

#include "dCommonVars.h"
#include "Logger.h"
#include "Game.h"

#include <unordered_map>
#include <vector>

enum class eAmf : uint8_t {
	Undefined = 0x00,            // An undefined AMF Value
	Null = 0x01,                 // A null AMF value
	False = 0x02,                // A false AMF value
	True = 0x03,                 // A true AMF value
	Integer = 0x04,              // An integer AMF value
	Double = 0x05,               // A double AMF value
	String = 0x06,               // A string AMF value
	XMLDoc = 0x07,               // Unused in the live client and cannot be serialized without modification.  An XML Doc AMF value
	Date = 0x08,                 // Unused in the live client and cannot be serialized without modification.  A date AMF value
	Array = 0x09,                // An array AMF value
	Object = 0x0A,               // Unused in the live client and cannot be serialized without modification.  An object AMF value
	XML = 0x0B,                  // Unused in the live client and cannot be serialized without modification.  An XML AMF value
	ByteArray = 0x0C,            // Unused in the live client and cannot be serialized without modification.  A byte array AMF value
	VectorInt = 0x0D,            // Unused in the live client and cannot be serialized without modification.  An integer vector AMF value
	VectorUInt = 0x0E,           // Unused in the live client and cannot be serialized without modification.  An unsigned integer AMF value
	VectorDouble = 0x0F,         // Unused in the live client and cannot be serialized without modification.  A double vector AMF value
	VectorObject = 0x10,         // Unused in the live client and cannot be serialized without modification.  An object vector AMF value
	Dictionary = 0x11            // Unused in the live client and cannot be serialized without modification.  A dictionary AMF value
};

class AMFBaseValue {
public:
	[[nodiscard]] constexpr virtual eAmf GetValueType() const noexcept { return eAmf::Undefined; }
	constexpr AMFBaseValue() noexcept = default;
	constexpr virtual ~AMFBaseValue() noexcept = default;
};

// AMFValue template class instantiations
template <typename ValueType>
class AMFValue : public AMFBaseValue {
public:
	AMFValue() = default;
	AMFValue(const ValueType value) : m_Data{ value } {}

	virtual ~AMFValue() override = default;

	[[nodiscard]] constexpr eAmf GetValueType() const noexcept override;

	[[nodiscard]] const ValueType& GetValue() const { return m_Data; }

	void SetValue(const ValueType value) { m_Data = value; }

protected:
	ValueType m_Data;
};

// Explicit template class instantiations
template class AMFValue<std::nullptr_t>;
template class AMFValue<bool>;
template class AMFValue<int32_t>;
template class AMFValue<uint32_t>;
template class AMFValue<std::string>;
template class AMFValue<double>;

// AMFValue template class member function instantiations
template <> [[nodiscard]] constexpr eAmf AMFValue<std::nullptr_t>::GetValueType() const noexcept { return eAmf::Null; }
template <> [[nodiscard]] constexpr eAmf AMFValue<bool>::GetValueType() const noexcept { return m_Data ? eAmf::True : eAmf::False; }
template <> [[nodiscard]] constexpr eAmf AMFValue<int32_t>::GetValueType() const noexcept { return eAmf::Integer; }
template <> [[nodiscard]] constexpr eAmf AMFValue<uint32_t>::GetValueType() const noexcept { return eAmf::Integer; }
template <> [[nodiscard]] constexpr eAmf AMFValue<std::string>::GetValueType() const noexcept { return eAmf::String; }
template <> [[nodiscard]] constexpr eAmf AMFValue<double>::GetValueType() const noexcept { return eAmf::Double; }

template <typename ValueType>
[[nodiscard]] constexpr eAmf AMFValue<ValueType>::GetValueType() const noexcept { return eAmf::Undefined; }

// As a string this is much easier to write and read from a BitStream.
template <>
class AMFValue<const char*> : public AMFBaseValue {
public:
	AMFValue() = default;
	AMFValue(const char* value) { m_Data = value; }
	virtual ~AMFValue() override = default;

	[[nodiscard]] constexpr eAmf GetValueType() const noexcept override { return eAmf::String; }

	[[nodiscard]] const std::string& GetValue() const { return m_Data; }
	void SetValue(const std::string& value) { m_Data = value; }
protected:
	std::string m_Data;
};

using AMFNullValue = AMFValue<std::nullptr_t>;
using AMFBoolValue = AMFValue<bool>;
using AMFIntValue = AMFValue<int32_t>;
using AMFStringValue = AMFValue<std::string>;
using AMFDoubleValue = AMFValue<double>;

/**
 * The AMFArrayValue object holds 2 types of lists:
 * An associative list where a key maps to a value
 * A Dense list where elements are stored back to back
 *
 * Objects that are Registered are owned by this object
 * and are not to be deleted by a caller.
 */
class AMFArrayValue : public AMFBaseValue {
	using AMFAssociative =
		std::unordered_map<std::string, std::unique_ptr<AMFBaseValue>, GeneralUtils::transparent_string_hash, std::equal_to<>>;

	using AMFDense = std::vector<std::unique_ptr<AMFBaseValue>>;

public:
	[[nodiscard]] constexpr eAmf GetValueType() const noexcept override { return eAmf::Array; }

	/**
	 * Returns the Associative portion of the object
	 */
	[[nodiscard]] inline const AMFAssociative& GetAssociative() const noexcept { return m_Associative; }

	/**
	 * Returns the dense portion of the object
	 */
	[[nodiscard]] inline const AMFDense& GetDense() const noexcept { return m_Dense; }

	/**
	 * Inserts an AMFValue into the associative portion with the given key.
	 * If a duplicate is attempted to be inserted, it is ignored and the
	 * first value with that key is kept in the map.
	 *
	 * These objects are not to be deleted by the caller as they are owned by
	 * the AMFArray object which manages its own memory.
	 *
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 *
	 * @return The inserted element if the type matched,
	 * or nullptr if a key existed and was not the same type
	 */
	template <typename ValueType>
	[[maybe_unused]] std::pair<AMFValue<ValueType>*, bool> Insert(const std::string_view key, const ValueType value) {
		const auto element = m_Associative.find(key);
		AMFValue<ValueType>* val = nullptr;
		bool found = true;
		if (element == m_Associative.cend()) {
			auto newVal = std::make_unique<AMFValue<ValueType>>(value);
			val = newVal.get();
			m_Associative.emplace(key, std::move(newVal));
		} else {
			val = dynamic_cast<AMFValue<ValueType>*>(element->second.get());
			found = false;
		}
		return std::make_pair(val, found);
	}

	// Associates an array with a string key
	[[maybe_unused]] std::pair<AMFBaseValue*, bool> Insert(const std::string_view key) {
		const auto element = m_Associative.find(key);
		AMFArrayValue* val = nullptr;
		bool found = true;
		if (element == m_Associative.cend()) {
			auto newVal = std::make_unique<AMFArrayValue>();
			val = newVal.get();
			m_Associative.emplace(key, std::move(newVal));
		} else {
			val = dynamic_cast<AMFArrayValue*>(element->second.get());
			found = false;
		}
		return std::make_pair(val, found);
	}

	// Associates an array with an integer key
	[[maybe_unused]] std::pair<AMFBaseValue*, bool> Insert(const size_t index) {
		bool inserted = false;
		if (index >= m_Dense.size()) {
			m_Dense.resize(index + 1);
			m_Dense.at(index) = std::make_unique<AMFArrayValue>();
			inserted = true;
		}
		return std::make_pair(dynamic_cast<AMFArrayValue*>(m_Dense.at(index).get()), inserted);
	}

	/**
	 * @brief Inserts an AMFValue into the AMFArray key'd by index.
	 * Attempting to insert the same key to the same value twice overwrites
	 * the previous value with the new one.
	 *
	 * @param index The index to associate with the value
	 * @param value The value to insert
	 * @return The inserted element, or nullptr if the type did not match
	 * what was at the index.
	 */
	template <typename ValueType>
	[[maybe_unused]] std::pair<AMFValue<ValueType>*, bool> Insert(const size_t index, const ValueType value) {
		bool inserted = false;
		if (index >= m_Dense.size()) {
			m_Dense.resize(index + 1);
			m_Dense.at(index) = std::make_unique<AMFValue<ValueType>>(value);
			inserted = true;
		}
		return std::make_pair(dynamic_cast<AMFValue<ValueType>*>(m_Dense.at(index).get()), inserted);
	}

	/**
	 * Inserts an AMFValue into the associative portion with the given key.
	 * If a duplicate is attempted to be inserted, it replaces the original
	 *
	 * The inserted element is now owned by this object and is not to be deleted
	 *
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 */
	void Insert(const std::string_view key, std::unique_ptr<AMFBaseValue> value) {
		const auto element = m_Associative.find(key);
		if (element != m_Associative.cend() && element->second) {
			element->second = std::move(value);
		} else {
			m_Associative.emplace(key, std::move(value));
		}
	}

	/**
	 * Inserts an AMFValue into the associative portion with the given index.
	 * If a duplicate is attempted to be inserted, it replaces the original
	 *
	 * The inserted element is now owned by this object and is not to be deleted
	 *
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 */
	void Insert(const size_t index, std::unique_ptr<AMFBaseValue> value) {
		if (index >= m_Dense.size()) {
			m_Dense.resize(index + 1);
		}
		m_Dense.at(index) = std::move(value);
	}

	/**
	 * Pushes an AMFValue into the back of the dense portion.
	 *
	 * These objects are not to be deleted by the caller as they are owned by
	 * the AMFArray object which manages its own memory.
	 *
	 * @param value The value to insert
	 *
	 * @return The inserted pointer, or nullptr should the key already be in use.
	 */
	template <typename ValueType>
	[[maybe_unused]] inline AMFValue<ValueType>* Push(const ValueType value) {
		return Insert(m_Dense.size(), value).first;
	}

	/**
	 * Removes the key from the associative portion
	 *
	 * The pointer removed is now no longer managed by this container
	 *
	 * @param key The key to remove from the associative portion
	 */
	void Remove(const std::string& key, const bool deleteValue = true) {
		const AMFAssociative::const_iterator it = m_Associative.find(key);
		if (it != m_Associative.cend()) {
			if (deleteValue) m_Associative.erase(it);
		}
	}

	/**
	 * Pops the last element in the dense portion, deleting it in the process.
	 */
	void Remove(const size_t index) {
		if (!m_Dense.empty() && index < m_Dense.size()) {
			const auto itr = m_Dense.cbegin() + index;
			m_Dense.erase(itr);
		}
	}

	void Pop() {
		if (!m_Dense.empty()) Remove(m_Dense.size() - 1);
	}

	[[nodiscard]] AMFArrayValue* GetArray(const std::string_view key) const {
		const AMFAssociative::const_iterator it = m_Associative.find(key);
		return it != m_Associative.cend() ? dynamic_cast<AMFArrayValue*>(it->second.get()) : nullptr;
	}

	[[nodiscard]] AMFArrayValue* GetArray(const size_t index) const {
		return index < m_Dense.size() ? dynamic_cast<AMFArrayValue*>(m_Dense.at(index).get()) : nullptr;
	}

	[[maybe_unused]] inline AMFArrayValue* InsertArray(const std::string_view key) {
		return static_cast<AMFArrayValue*>(Insert(key).first);
	}

	[[maybe_unused]] inline AMFArrayValue* InsertArray(const size_t index) {
		return static_cast<AMFArrayValue*>(Insert(index).first);
	}

	[[maybe_unused]] inline AMFArrayValue* PushArray() {
		return static_cast<AMFArrayValue*>(Insert(m_Dense.size()).first);
	}

	/**
	 * Gets an AMFValue by the key from the associative portion and converts it
	 * to the AmfValue template type.  If the key did not exist, it is inserted.
	 *
	 * @tparam The target object type
	 * @param key The key to lookup
	 *
	 * @return The AMFValue
	 */
	template <typename AmfType>
	[[nodiscard]] AMFValue<AmfType>* Get(const std::string_view key) const {
		const AMFAssociative::const_iterator it = m_Associative.find(key);
		return it != m_Associative.cend() ?
			dynamic_cast<AMFValue<AmfType>*>(it->second.get()) :
			nullptr;
	}

	// Get from the array but dont cast it
	[[nodiscard]] AMFBaseValue* Get(const std::string_view key) const {
		const AMFAssociative::const_iterator it = m_Associative.find(key);
		return it != m_Associative.cend() ? it->second.get() : nullptr;
	}

	/**
	 * @brief Get an AMFValue object at a position in the dense portion.
	 * Gets an AMFValue by the index from the dense portion and converts it
	 * to the AmfValue template type.  If the index did not exist, it is inserted.
	 *
	 * @tparam The target object type
	 * @param index The index to get
	 * @return The casted object, or nullptr.
	 */
	template <typename AmfType>
	[[nodiscard]] AMFValue<AmfType>* Get(const size_t index) const {
		return index < m_Dense.size() ?
			dynamic_cast<AMFValue<AmfType>*>(m_Dense.at(index).get()) :
			nullptr;
	}

	// Get from the dense but dont cast it
	[[nodiscard]] AMFBaseValue* Get(const size_t index) const {
		return index < m_Dense.size() ? m_Dense.at(index).get() : nullptr;
	}

private:
	/**
	 * The associative portion.  These values are key'd with strings to an AMFValue.
	 */
	AMFAssociative m_Associative;

	/**
	 * The dense portion.  These AMFValue's are stored one after
	 * another with the most recent addition being at the back.
	 */
	AMFDense m_Dense;
};

#endif  //!__AMF3__H__
