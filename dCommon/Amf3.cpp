#include "Amf3.h"

#include "Game.h"
#include "dLogger.h"

AMFArrayValue::~AMFArrayValue() {
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
}

bool AMFArrayValue::InsertAssociative(const std::string& key, AMFValue* value) {
	bool existingValue = this->associative.find(key) != this->associative.end();
	if (existingValue) {
		Game::logger->Log("AMFArrayValue",
			"Warning: Failed to insert key %s into associative portion.  Object will not be managed!", key.c_str());
		return false;
	}
	this->associative.insert(std::make_pair(key, value));
	return true;
}


AMFUndefinedValue* AMFArrayValue::InsertAssociativeUndefined(const std::string& key) {
	AMFUndefinedValue* toInsert = new AMFUndefinedValue();
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFNullValue* AMFArrayValue::InsertAssociativeNull(const std::string& key) {
	AMFNullValue* toInsert = new AMFNullValue();
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFArrayValue* AMFArrayValue::InsertAssociativeArray(const std::string& key) {
	AMFArrayValue* toInsert = new AMFArrayValue();
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFValue* AMFArrayValue::InsertAssociative(const std::string& key, const bool value) {
	AMFValue* toInsert = value ? static_cast<AMFValue*>(new AMFTrueValue()) : static_cast<AMFValue*>(new AMFFalseValue());
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFStringValue* AMFArrayValue::InsertAssociative(const std::string& key, const char* value) {
	AMFStringValue* toInsert = new AMFStringValue(value);
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFIntegerValue* AMFArrayValue::InsertAssociative(const std::string& key, const uint32_t value) {
	AMFIntegerValue* toInsert = new AMFIntegerValue(value);
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

AMFDoubleValue* AMFArrayValue::InsertAssociative(const std::string& key, const double value) {
	AMFDoubleValue* toInsert = new AMFDoubleValue(value);
	if (!InsertAssociative(key, toInsert)) {
		delete toInsert;
		return nullptr;
	}
	return toInsert;
}

void AMFArrayValue::PushBackDense(AMFValue* valueToPush) {
	this->dense.push_back(valueToPush);
}

AMFUndefinedValue* AMFArrayValue::PushDenseUndefined() {
	AMFUndefinedValue* toPush = new AMFUndefinedValue();
	PushBackDense(toPush);
	return toPush;
}

AMFNullValue* AMFArrayValue::PushDenseNull() {
	AMFNullValue* toPush = new AMFNullValue();
	PushBackDense(toPush);
	return toPush;
}

AMFArrayValue* AMFArrayValue::PushDenseArray() {
	AMFArrayValue* toPush = new AMFArrayValue();
	PushBackDense(toPush);
	return toPush;
}

AMFStringValue* AMFArrayValue::PushDense(const char* value) {
	AMFStringValue* toPush = new AMFStringValue(value);
	PushBackDense(toPush);
	return toPush;
}

AMFValue* AMFArrayValue::PushDense(const bool value) {
	AMFValue* toPush = value ? static_cast<AMFValue*>(new AMFTrueValue()) : static_cast<AMFValue*>(new AMFFalseValue());
	PushBackDense(toPush);
	return toPush;
}

AMFIntegerValue* AMFArrayValue::PushDense(const uint32_t value) {
	AMFIntegerValue* toPush = new AMFIntegerValue(value);
	PushBackDense(toPush);
	return toPush;
}

AMFDoubleValue* AMFArrayValue::PushDense(const double value) {
	AMFDoubleValue* toPush = new AMFDoubleValue(value);
	PushBackDense(toPush);
	return toPush;
}

