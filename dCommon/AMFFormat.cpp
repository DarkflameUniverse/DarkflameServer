#include "AMFFormat.h"

// AMFInteger
void AMFIntegerValue::SetIntegerValue(uint32_t value) {
	this->value = value;
}

uint32_t AMFIntegerValue::GetIntegerValue() {
	return this->value;
}

// AMFDouble
void AMFDoubleValue::SetDoubleValue(double value) {
	this->value = value;
}

double AMFDoubleValue::GetDoubleValue() {
	return this->value;
}

// AMFString
void AMFStringValue::SetStringValue(const std::string& value) {
	this->value = value;
}

std::string AMFStringValue::GetStringValue() {
	return this->value;
}

// AMFXMLDoc
void AMFXMLDocValue::SetXMLDocValue(const std::string& value) {
	this->xmlData = value;
}

std::string AMFXMLDocValue::GetXMLDocValue() {
	return this->xmlData;
}

// AMFDate
void AMFDateValue::SetDateValue(uint64_t value) {
	this->millisecondTimestamp = value;
}

uint64_t AMFDateValue::GetDateValue() {
	return this->millisecondTimestamp;
}

// AMFArray Insert Value
void AMFArrayValue::InsertValue(const std::string& key, AMFValue* value) {
	this->associative.insert(std::make_pair(key, value));
}

// AMFArray Remove Value
void AMFArrayValue::RemoveValue(const std::string& key) {
	_AMFArrayMap_::iterator it = this->associative.find(key);
	if (it != this->associative.end()) {
		this->associative.erase(it);
	}
}

// AMFArray Find Value
AMFValue* AMFArrayValue::FindValue(const std::string& key) {
	_AMFArrayMap_::iterator it = this->associative.find(key);
	if (it != this->associative.end()) {
		return it->second;
	}

	return nullptr;
}

// AMFArray Get Associative Iterator Begin
_AMFArrayMap_::iterator AMFArrayValue::GetAssociativeIteratorValueBegin() {
	return this->associative.begin();
}

// AMFArray Get Associative Iterator End
_AMFArrayMap_::iterator AMFArrayValue::GetAssociativeIteratorValueEnd() {
	return this->associative.end();
}

// AMFArray Push Back Value
void AMFArrayValue::PushBackValue(AMFValue* value) {
	this->dense.push_back(value);
}

// AMFArray Pop Back Value
void AMFArrayValue::PopBackValue() {
	this->dense.pop_back();
}

// AMFArray Get Dense List Size
uint32_t AMFArrayValue::GetDenseValueSize() {
	return (uint32_t)this->dense.size();
}

// AMFArray Get value at index in Dense List
AMFValue* AMFArrayValue::GetValueAt(uint32_t index) {
	return this->dense.at(index);
}

// AMFArray Get Dense Iterator Begin
_AMFArrayList_::iterator AMFArrayValue::GetDenseIteratorBegin() {
	return this->dense.begin();
}

// AMFArray Get Dense Iterator End
_AMFArrayList_::iterator AMFArrayValue::GetDenseIteratorEnd() {
	return this->dense.end();
}

AMFArrayValue::~AMFArrayValue() {
	for (auto valueToDelete : GetDenseArray()) {
		if (valueToDelete) delete valueToDelete;
	}
	for (auto valueToDelete : GetAssociativeMap()) {
		if (valueToDelete.second) delete valueToDelete.second;
	}
}

// AMFObject Constructor
AMFObjectValue::AMFObjectValue(std::vector<std::pair<std::string, AMFValueType>> traits) {
	this->traits.reserve(traits.size());
	std::vector<std::pair<std::string, AMFValueType>>::iterator it = traits.begin();
	while (it != traits.end()) {
		this->traits.insert(std::make_pair(it->first, std::make_pair(it->second, new AMFNullValue())));
		it++;
	}
}

// AMFObject Set Value
void AMFObjectValue::SetTraitValue(const std::string& trait, AMFValue* value) {
	if (value) {
		_AMFObjectTraits_::iterator it = this->traits.find(trait);
		if (it != this->traits.end()) {
			if (it->second.first == value->GetValueType()) {
				it->second.second = value;
			}
		}
	}
}

// AMFObject Get Value
AMFValue* AMFObjectValue::GetTraitValue(const std::string& trait) {
	_AMFObjectTraits_::iterator it = this->traits.find(trait);
	if (it != this->traits.end()) {
		return it->second.second;
	}

	return nullptr;
}

// AMFObject Get Trait Iterator Begin
_AMFObjectTraits_::iterator AMFObjectValue::GetTraitsIteratorBegin() {
	return this->traits.begin();
}

// AMFObject Get Trait Iterator End
_AMFObjectTraits_::iterator AMFObjectValue::GetTraitsIteratorEnd() {
	return this->traits.end();
}

// AMFObject Get Trait Size
uint32_t AMFObjectValue::GetTraitArrayCount() {
	return (uint32_t)this->traits.size();
}

AMFObjectValue::~AMFObjectValue() {
	for (auto valueToDelete = GetTraitsIteratorBegin(); valueToDelete != GetTraitsIteratorEnd(); valueToDelete++) {
		if (valueToDelete->second.second) delete valueToDelete->second.second;
	}
}
