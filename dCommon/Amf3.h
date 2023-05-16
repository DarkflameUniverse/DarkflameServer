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
	virtual eAmf GetValueType() { return eAmf::Undefined; };
	AMFBaseValue() {};
	virtual ~AMFBaseValue() {};
};

template<typename ValueType>
class AMFValue : public AMFBaseValue {
public:
	AMFValue() {};
	AMFValue(ValueType value) { SetValue(value); };
	virtual ~AMFValue() override {};

	eAmf GetValueType() override { return eAmf::Undefined; };

	const ValueType& GetValue() { return data; };
	void SetValue(ValueType value) { data = value; };
protected:
	ValueType data;
};

// As a string this is much easier to write and read from a BitStream.
template<>
class AMFValue<const char*> : public AMFBaseValue {
public:
	AMFValue() {};
	AMFValue(const char* value) { SetValue(std::string(value)); };
	virtual ~AMFValue() override {};

	eAmf GetValueType() override { return eAmf::String; };

	const std::string& GetValue() { return data; };
	void SetValue(std::string value) { data = value; };
protected:
	std::string data;
};

typedef AMFValue<std::nullptr_t> AMFNullValue;
typedef AMFValue<bool> AMFBoolValue;
typedef AMFValue<int32_t> AMFIntValue;
typedef AMFValue<std::string> AMFStringValue;
typedef AMFValue<double> AMFDoubleValue;

template<> inline eAmf AMFValue<std::nullptr_t>::GetValueType() { return eAmf::Null; };
template<> inline eAmf AMFValue<bool>::GetValueType() { return this->data ? eAmf::True : eAmf::False; };
template<> inline eAmf AMFValue<int32_t>::GetValueType() { return eAmf::Integer; };
template<> inline eAmf AMFValue<uint32_t>::GetValueType() { return eAmf::Integer; };
template<> inline eAmf AMFValue<std::string>::GetValueType() { return eAmf::String; };
template<> inline eAmf AMFValue<double>::GetValueType() { return eAmf::Double; };

/**
 * The AMFArrayValue object holds 2 types of lists:
 * An associative list where a key maps to a value
 * A Dense list where elements are stored back to back
 *
 * Objects that are Registered are owned by this object
 * and are not to be deleted by a caller.
 */
class AMFArrayValue : public AMFBaseValue {

	typedef std::unordered_map<std::string, AMFBaseValue*> AMFAssociative;
	typedef std::vector<AMFBaseValue*> AMFDense;

public:
	eAmf GetValueType() override { return eAmf::Array; };

	~AMFArrayValue() override {
		for (auto valueToDelete : GetDense()) {
			if (valueToDelete) {
				delete valueToDelete;
				valueToDelete = nullptr;
			}
		}
		for (auto valueToDelete : GetAssociative()) {
			if (valueToDelete.second) {
				delete valueToDelete.second;
				valueToDelete.second = nullptr;
			}
		}
	};

	/**
	 * Returns the Associative portion of the object
	 */
	inline AMFAssociative& GetAssociative() { return this->associative; };

	/**
	 * Returns the dense portion of the object
	 */
	inline AMFDense& GetDense() { return this->dense; };

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
	template<typename ValueType>
	std::pair<AMFValue<ValueType>*, bool> Insert(const std::string& key, ValueType value) {
		auto element = associative.find(key);
		AMFValue<ValueType>* val = nullptr;
		bool found = true;
		if (element == associative.end()) {
			val = new AMFValue<ValueType>(value);
			associative.insert(std::make_pair(key, val));
		} else {
			val = dynamic_cast<AMFValue<ValueType>*>(element->second);
			found = false;
		}
		return std::make_pair(val, found);
	};

	// Associates an array with a string key
	std::pair<AMFBaseValue*, bool> Insert(const std::string& key) {
		auto element = associative.find(key);
		AMFArrayValue* val = nullptr;
		bool found = true;
		if (element == associative.end()) {
			val = new AMFArrayValue();
			associative.insert(std::make_pair(key, val));
		} else {
			val = dynamic_cast<AMFArrayValue*>(element->second);
			found = false;
		}
		return std::make_pair(val, found);
	};

	// Associates an array with an integer key
	std::pair<AMFBaseValue*, bool> Insert(const uint32_t& index) {
		AMFArrayValue* val = nullptr;
		bool inserted = false;
		if (index >= dense.size()) {
			dense.resize(index + 1);
			val = new AMFArrayValue();
			dense.at(index) = val;
			inserted = true;
		}
		return std::make_pair(dynamic_cast<AMFArrayValue*>(dense.at(index)), inserted);
	};

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
	template<typename ValueType>
	std::pair<AMFValue<ValueType>*, bool> Insert(const uint32_t& index, ValueType value) {
		AMFValue<ValueType>* val = nullptr;
		bool inserted = false;
		if (index >= this->dense.size()) {
			this->dense.resize(index + 1);
			val = new AMFValue<ValueType>(value);
			this->dense.at(index) = val;
			inserted = true;
		}
		return std::make_pair(dynamic_cast<AMFValue<ValueType>*>(this->dense.at(index)), inserted);
	};

	/**
	 * Inserts an AMFValue into the associative portion with the given key.
	 * If a duplicate is attempted to be inserted, it replaces the original
	 *
	 * The inserted element is now owned by this object and is not to be deleted
	 *
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 */
	void Insert(const std::string& key, AMFBaseValue* value) {
		auto element = associative.find(key);
		if (element != associative.end() && element->second) {
			delete element->second;
			element->second = value;
		} else {
			associative.insert(std::make_pair(key, value));
		}
	};

	/**
	 * Inserts an AMFValue into the associative portion with the given index.
	 * If a duplicate is attempted to be inserted, it replaces the original
	 *
	 * The inserted element is now owned by this object and is not to be deleted
	 *
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 */
	void Insert(const uint32_t index, AMFBaseValue* value) {
		if (index < dense.size()) {
			AMFDense::iterator itr = dense.begin() + index;
			if (*itr) delete dense.at(index);
		} else {
			dense.resize(index + 1);
		}
		dense.at(index) = value;
	};

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
	template<typename ValueType>
	inline AMFValue<ValueType>* Push(ValueType value) {
		return Insert(this->dense.size(), value).first;
	};

	/**
	 * Removes the key from the associative portion
	 *
	 * The pointer removed is now no longer managed by this container
	 *
	 * @param key The key to remove from the associative portion
	 */
	void Remove(const std::string& key, bool deleteValue = true) {
		AMFAssociative::iterator it = this->associative.find(key);
		if (it != this->associative.end()) {
			if (deleteValue) delete it->second;
			this->associative.erase(it);
		}
	}

	/**
	 * Pops the last element in the dense portion, deleting it in the process.
	 */
	void Remove(const uint32_t index) {
		if (!this->dense.empty() && index < this->dense.size()) {
			auto itr = this->dense.begin() + index;
			if (*itr) delete (*itr);
			this->dense.erase(itr);
		}
	}

	void Pop() {
		if (!this->dense.empty()) Remove(this->dense.size() - 1);
	}

	AMFArrayValue* GetArray(const std::string& key) {
		AMFAssociative::const_iterator it = this->associative.find(key);
		if (it != this->associative.end()) {
			return dynamic_cast<AMFArrayValue*>(it->second);
		}
		return nullptr;
	};

	AMFArrayValue* GetArray(const uint32_t index) {
		return index >= this->dense.size() ? nullptr : dynamic_cast<AMFArrayValue*>(this->dense.at(index));
	};

	inline AMFArrayValue* InsertArray(const std::string& key) {
		return static_cast<AMFArrayValue*>(Insert(key).first);
	};

	inline AMFArrayValue* InsertArray(const uint32_t index) {
		return static_cast<AMFArrayValue*>(Insert(index).first);
	};

	inline AMFArrayValue* PushArray() {
		return static_cast<AMFArrayValue*>(Insert(this->dense.size()).first);
	};

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
	AMFValue<AmfType>* Get(const std::string& key) const {
		AMFAssociative::const_iterator it = this->associative.find(key);
		return it != this->associative.end() ?
			dynamic_cast<AMFValue<AmfType>*>(it->second) :
			nullptr;
	};

	// Get from the array but dont cast it
	AMFBaseValue* Get(const std::string& key) const {
		AMFAssociative::const_iterator it = this->associative.find(key);
		return it != this->associative.end() ? it->second : nullptr;
	};

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
	AMFValue<AmfType>* Get(uint32_t index) const {
		std::cout << (index < this->dense.size()) << std::endl;
		return index < this->dense.size() ?
			dynamic_cast<AMFValue<AmfType>*>(this->dense.at(index)) :
			nullptr;
	};

	// Get from the dense but dont cast it
	AMFBaseValue* Get(const uint32_t index) const {
		return index < this->dense.size() ? this->dense.at(index) : nullptr;
	};
private:
	/**
	 * The associative portion.  These values are key'd with strings to an AMFValue.
	 */
	AMFAssociative associative;

	/**
	 * The dense portion.  These AMFValue's are stored one after
	 * another with the most recent addition being at the back.
	 */
	AMFDense dense;
};

#endif  //!__AMF3__H__
