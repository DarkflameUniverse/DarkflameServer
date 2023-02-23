#ifndef __AMF3__H__
#define __AMF3__H__

#include "dCommonVars.h"

#include <unordered_map>
#include <vector>

enum class eAmf : unsigned char {
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

class AMFValue {
public:
	virtual eAmf GetValueType() { return static_cast<eAmf>(-1); };
	virtual ~AMFValue() {};
};

class AMFUndefinedValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::Undefined;
	eAmf GetValueType() override { return ValueType; };
};

class AMFNullValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::Null;
	eAmf GetValueType() override { return ValueType; };
};

class AMFFalseValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::False;
	eAmf GetValueType() override { return ValueType; };
};

class AMFTrueValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::True;
	eAmf GetValueType() override { return ValueType; };

};

class AMFIntegerValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::Integer;
	eAmf GetValueType() override { return ValueType; };

	AMFIntegerValue() { this->data = 0; };
	AMFIntegerValue(uint32_t value) { this->data = value; };
	/**
	 * Sets the value of this AMFInteger object.
	 * Note that this value cannot exceed 29 bits.
	 * Note this value can be sign extended in some cases.
	 *
	 * @param value The data to set
	 */
	inline void SetValue(uint32_t value) { this->data = value; };
	inline uint32_t GetValue() { return this->data; };

private:
	// Implement const's for the min and max values for the value.

	// This objects data represented as a U32
	uint32_t data;
};

//! The double value AMF type
class AMFDoubleValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::Double;
	eAmf GetValueType() override { return ValueType; };

	AMFDoubleValue() { this->data = 0.0; };
	AMFDoubleValue(double value) { this->data = value; };

	inline void SetValue(double value) { this->data = value; };
	inline double GetValue() { return this->data; };

private:
	// The data for this amf object
	double data;
};

class AMFStringValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::String;
	eAmf GetValueType() override { return ValueType; };

	AMFStringValue() { this->data = ""; };
	AMFStringValue(const std::string& value) { this->data = value; };

	inline void SetValue(const std::string& value) { this->data = value; };
	inline std::string GetValue() { return this->data; };

private:

	// The data for this object
	std::string data;
};

typedef std::unordered_map<std::string, AMFValue*> _AMFAssociativePortion_;
typedef std::vector<AMFValue*> _AMFDensePortion_;

/**
 * The AMFArrayValue object holds 2 types of lists:
 * An associative list where a key maps to a value
 * A Dense list where elements are stored back to back
 *
 * Objects that are Registered are owned by this object
 * and are not to be deleted by a caller.
 */
class AMFArrayValue : public AMFValue {
public:
	static const eAmf ValueType = eAmf::Array;
	eAmf GetValueType() override { return ValueType; }

	~AMFArrayValue() override;

	/**
	 * Returns the Associative portion of the object
	 */
	inline _AMFAssociativePortion_& GetAssociative() { return this->associative; };

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
	 * @return The inserted pointer, or nullptr should the key already be in use.
	 */

	// Special definition for these two which have no data
	AMFUndefinedValue* InsertAssociativeUndefined(const std::string& key);
	AMFNullValue* InsertAssociativeNull(const std::string& key);

	AMFArrayValue* InsertAssociativeArray(const std::string& key);
	AMFStringValue* InsertAssociative(const std::string& key, const std::string& value = "");
	AMFValue* InsertAssociative(const std::string& key, const bool& value);
	AMFIntegerValue* InsertAssociative(const std::string& key, const uint32_t& value = 0);
	AMFDoubleValue* InsertAssociative(const std::string& key, const double& value = 0.0);

	/**
	 * Register an AMFValue* to this Object.
	 * 
	 * The inserted pointer is now owned by this object and should not be deleted.
	 * 
	 * @param key The key associate with the pointer
	 * @param value The value to insert
	 * 
	 * @return True if the object was inserted successfully, false otherwise.
	 * If this returns false, the inserted pointer cannot be managed by this object
	 * with that key due to colliding keys!
	 */
	bool RegisterAssociative(const std::string& key, AMFValue* value) { return InsertAssociative(key, value); };

	/**
	 * Removes the key from the associative portion
	 * 
	 * The pointer removed is now no longer managed by this container
	 *
	 * @param key The key to remove from the associative portion
	 */
	void RemoveAssociative(const std::string& key, bool deleteValue = true) {
		_AMFAssociativePortion_::iterator it = this->associative.find(key);
		if (it != this->associative.end()) {
			if (deleteValue) delete it->second;
			this->associative.erase(it);
		}
	}

	/**
	 * Returns the dense portion of the object
	 */
	inline _AMFDensePortion_& GetDense() { return this->dense; };

	/**
	 * Inserts a AMFValue at the back of the dense portion.
	 *
	 * This object is not to be deleted by the caller as it is now owned by the AMFArray object
	 * which manages its own memory.
	 *
	 * @param value The value to push back
	 */
	inline void RegisterDense(AMFValue* value) { this->dense.push_back(value); };

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

	// Special definition for these two which have no data
	AMFUndefinedValue* PushDenseUndefined();
	AMFNullValue* PushDenseNull();

	AMFArrayValue* PushDenseArray();
	AMFStringValue* PushDense(const std::string& value = "");
	AMFValue* PushDense(const bool& value);
	AMFIntegerValue* PushDense(const uint32_t& value = 0);
	AMFDoubleValue* PushDense(const double& value = 0.0);

	/**
	 * Pops the last element in the dense portion, deleting it in the process.
	 */
	void PopDense() {
		if (!this->dense.empty()) {
			AMFValue* back = this->dense.back();
			if (back) delete back;
			this->dense.pop_back();
		}
	}

	// Templates

	/**
	 * Gets an AMFValue by the key from the associative portion and converts it to the template type.
	 * If the key does not exist or the AMFValue's type does not match the template
	 * nullptr is returned.
	 *
	 * @tparam The target object type
	 * @param key The key to lookup
	 * @return The casted AMFValue, or nullptr.
	 */
	template <typename T>
	T* FindValue(const std::string& key) const {
		_AMFAssociativePortion_::const_iterator it = this->associative.find(key);
		if (it != this->associative.end() && T::ValueType == it->second->GetValueType()) {
			return dynamic_cast<T*>(it->second);
		}

		return nullptr;
	};

	/**
	 * @brief Get an AMFValue object at a position in the dense portion.
	 * If the template type does not match, or the index is out of bounds, nullptr is returned.
	 *
	 * @tparam The target object type
	 * @param index The index to get
	 * @return The casted object, or nullptr.
	 */
	template <typename T>
	T* GetValueAt(uint32_t index) {
		if (index >= this->dense.size()) return nullptr;
		AMFValue* foundValue = this->dense.at(index);
		return T::ValueType == foundValue->GetValueType() ? dynamic_cast<T*>(foundValue) : nullptr;
	};
private:
	/**
	 * Inserts an AMFValue into the map if it doesn't exist.
	 * 
	 * @param key The key to associate with the value
	 * @param value The value to insert
	 * 
	 * @return True if the value was inserted successfully, false otherwise.
	 */
	bool InsertAssociative(const std::string& key, AMFValue* value);

	/**
	 * Pushes a value back into the dense portion
	 * 
	 * @param valueToPush The value to push into the dense portion
	 */
	void PushBackDense(AMFValue* valueToPush);

	/**
	 * The associative portion.  These values are key'd with strings to an AMFValue.
	 */
	_AMFAssociativePortion_ associative;

	/**
	 * The dense portion.  These AMFValue's are stored one after
	 * another with the most recent addition being at the back.
	 */
	_AMFDensePortion_ dense;
};

#endif  //!__AMF3__H__
