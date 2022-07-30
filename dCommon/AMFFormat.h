#pragma once

// Custom Classes
#include "dCommonVars.h"

// C++
#include <unordered_map>
#include <vector>

/*!
  \file AMFFormat.hpp
  \brief A class for managing AMF values
 */

class AMFValue;     // Forward declaration

// Definitions
#define _AMFArrayMap_               std::unordered_map<std::string, AMFValue*>
#define _AMFArrayList_              std::vector<AMFValue*>

#define _AMFObjectTraits_           std::unordered_map<std::string, std::pair<AMFValueType, AMFValue*>>
#define _AMFObjectDynamicTraits_    std::unordered_map<std::string, AMFValue*>

//! An enum for each AMF value type
enum AMFValueType : unsigned char {
	AMFUndefined = 0x00,            //!< An undefined AMF Value
	AMFNull = 0x01,                 //!< A null AMF value
	AMFFalse = 0x02,                //!< A false AMF value
	AMFTrue = 0x03,                 //!< A true AMF value
	AMFInteger = 0x04,              //!< An integer AMF value
	AMFDouble = 0x05,               //!< A double AMF value
	AMFString = 0x06,               //!< A string AMF value
	AMFXMLDoc = 0x07,               //!< An XML Doc AMF value
	AMFDate = 0x08,                 //!< A date AMF value
	AMFArray = 0x09,                //!< An array AMF value
	AMFObject = 0x0A,               //!< An object AMF value
	AMFXML = 0x0B,                  //!< An XML AMF value
	AMFByteArray = 0x0C,            //!< A byte array AMF value
	AMFVectorInt = 0x0D,            //!< An integer vector AMF value
	AMFVectorUInt = 0x0E,           //!< An unsigned integer AMF value
	AMFVectorDouble = 0x0F,         //!< A double vector AMF value
	AMFVectorObject = 0x10,         //!< An object vector AMF value
	AMFDictionary = 0x11            //!< A dictionary AMF value
};

//! An enum for the object value types
enum AMFObjectValueType : unsigned char {
	AMFObjectAnonymous = 0x01,
	AMFObjectTyped = 0x02,
	AMFObjectDynamic = 0x03,
	AMFObjectExternalizable = 0x04
};

//! The base AMF value class
class AMFValue {
public:
	//! Returns the AMF value type
	/*!
	  \return The AMF value type
	 */
	virtual AMFValueType GetValueType() = 0;
	virtual ~AMFValue() {};
};

//! A typedef for a pointer to an AMF value
typedef AMFValue* NDGFxValue;


// The various AMF value types

//! The undefined value AMF type
class AMFUndefinedValue : public AMFValue {
private:
	//! Returns the AMF value type
	/*!
	  \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFUndefined; }
};

//! The null value AMF type
class AMFNullValue : public AMFValue {
private:
	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFNull; }
};

//! The false value AMF type
class AMFFalseValue : public AMFValue {
private:
	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFFalse; }
};

//! The true value AMF type
class AMFTrueValue : public AMFValue {
private:
	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFTrue; }
};

//! The integer value AMF type
class AMFIntegerValue : public AMFValue {
private:
	uint32_t value;         //!< The value of the AMF type

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFInteger; }

public:
	//! Sets the integer value
	/*!
	  \param value The value to set
	 */
	void SetIntegerValue(uint32_t value);

	//! Gets the integer value
	/*!
	  \return The integer value
	 */
	uint32_t GetIntegerValue();
};

//! The double value AMF type
class AMFDoubleValue : public AMFValue {
private:
	double value;           //!< The value of the AMF type

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFDouble; }

public:
	//! Sets the double value
	/*!
	  \param value The value to set to
	 */
	void SetDoubleValue(double value);

	//! Gets the double value
	/*!
	  \return The double value
	 */
	double GetDoubleValue();
};

//! The string value AMF type
class AMFStringValue : public AMFValue {
private:
	std::string value;          //!< The value of the AMF type

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFString; }

public:
	//! Sets the string value
	/*!
	  \param value The string value to set to
	 */
	void SetStringValue(const std::string& value);

	//! Gets the string value
	/*!
	  \return The string value
	 */
	std::string GetStringValue();
};

//! The XML doc value AMF type
class AMFXMLDocValue : public AMFValue {
private:
	std::string xmlData;            //!< The value of the AMF type

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFXMLDoc; }

public:
	//! Sets the XML Doc value
	/*!
	  \param value The value to set to
	 */
	void SetXMLDocValue(const std::string& value);

	//! Gets the XML Doc value
	/*!
	  \return The XML Doc value
	 */
	std::string GetXMLDocValue();
};

//! The date value AMF type
class AMFDateValue : public AMFValue {
private:
	uint64_t millisecondTimestamp;      //!< The time in milliseconds since the ephoch

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFDate; }

public:
	//! Sets the date time
	/*!
	  \param value The value to set to
	 */
	void SetDateValue(uint64_t value);

	//! Gets the date value
	/*!
	  \return The date value in milliseconds since the epoch
	 */
	uint64_t GetDateValue();
};

//! The array value AMF type
// This object will manage it's own memory map and list.  Do not delete its values.
class AMFArrayValue : public AMFValue {
private:
	_AMFArrayMap_ associative;      //!< The array map (associative part)
	_AMFArrayList_ dense;           //!< The array list (dense part)

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFArray; }

public:
	~AMFArrayValue() override;
	//! Inserts an item into the array map for a specific key
	/*!
	  \param key The key to set
	  \param value The value to add
	 */
	void InsertValue(const std::string& key, AMFValue* value);

	//! Removes an item for a specific key
	/*!
	  \param key The key to remove
	 */
	void RemoveValue(const std::string& key);

	//! Finds an AMF value
	/*!
	  \return The AMF value if found, nullptr otherwise
	 */
	AMFValue* FindValue(const std::string& key);

	//! Returns where the associative iterator begins
	/*!
	  \return Where the array map iterator begins
	 */
	_AMFArrayMap_::iterator GetAssociativeIteratorValueBegin();

	//! Returns where the associative iterator ends
	/*!
	  \return Where the array map iterator ends
	 */
	_AMFArrayMap_::iterator GetAssociativeIteratorValueEnd();

	//! Pushes back a value into the array list
	/*!
	  \param value The value to push back
	 */
	void PushBackValue(AMFValue* value);

	//! Pops back the last value in the array list
	void PopBackValue();

	//! Gets the count of the dense list
	/*!
	  \return The dense list size
	 */
	uint32_t GetDenseValueSize();

	//! Gets a specific value from the list for the specified index
	/*!
	  \param index The index to get
	 */
	AMFValue* GetValueAt(uint32_t index);

	//! Returns where the dense iterator begins
	/*!
	  \return Where the iterator begins
	 */
	_AMFArrayList_::iterator GetDenseIteratorBegin();

	//! Returns where the dense iterator ends
	/*!
	  \return Where the iterator ends
	 */
	_AMFArrayList_::iterator GetDenseIteratorEnd();

	//! Returns the associative map
	/*!
	  \return The associative map
	 */
	_AMFArrayMap_ GetAssociativeMap() { return this->associative; };

	//! Returns the dense array
	/*!
	  \return The dense array
	 */
	_AMFArrayList_ GetDenseArray() { return this->dense; };
};

//! The anonymous object value AMF type
class AMFObjectValue : public AMFValue {
private:
	_AMFObjectTraits_ traits;           //!< The object traits

	//! Returns the AMF value type
	/*!
	 \return The AMF value type
	 */
	AMFValueType GetValueType() { return AMFObject; }
	~AMFObjectValue() override;

public:
	//! Constructor
	/*!
	  \param traits The traits to set
	 */
	AMFObjectValue(std::vector<std::pair<std::string, AMFValueType>> traits);

	//! Gets the object value type
	/*!
	  \return The object value type
	 */
	virtual AMFObjectValueType GetObjectValueType() { return AMFObjectAnonymous; }

	//! Sets the value of a trait
	/*!
	  \param trait The trait to set the value for
	  \param value The AMF value to set
	 */
	void SetTraitValue(const std::string& trait, AMFValue* value);

	//! Gets a trait value
	/*!
	  \param trait The trait to get the value for
	  \return The trait value
	 */
	AMFValue* GetTraitValue(const std::string& trait);

	//! Gets the beginning of the object traits iterator
	/*!
	  \return The AMF trait array iterator begin
	 */
	_AMFObjectTraits_::iterator GetTraitsIteratorBegin();

	//! Gets the end of the object traits iterator
	/*!
	  \return The AMF trait array iterator begin
	 */
	_AMFObjectTraits_::iterator GetTraitsIteratorEnd();

	//! Gets the amount of traits
	/*!
	  \return The amount of traits
	 */
	uint32_t GetTraitArrayCount();
};
