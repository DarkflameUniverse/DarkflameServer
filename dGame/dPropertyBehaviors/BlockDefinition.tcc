#ifndef __BLOCKDEFINITION__H__

#error Do not include this file directly.  Include BlockDefinition.h instead.

#endif //! __BLOCKDEFINITION__H__

#include <string>

#include "AMFFormat.h"
#include "BitStream.h"
#include "BlockDefinition.h"
#include "Game.h"
#include "dLogger.h"

template<>
class BehaviorBlock<std::string> : public BlockBase {
public:
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition) : BlockBase(name, typeName) {
		this->baseDefinition = baseDefinition;
	};

	void SerializeToAmf(AMFArrayValue* bitStream) override {
		Game::logger->Log("BehaviorBlock", "Overloaded serialize for string!");
		BlockBase::SerializeToAmf(bitStream);
		AMFStringValue* serializedValue = new AMFStringValue();
		serializedValue->SetStringValue(GetCurrentValue());
		bitStream->InsertValue(typeName, serializedValue);
	};

	std::string GetCurrentValue() { return currentValue; };
private:
	std::string currentValue;
	BlockDefinition* baseDefinition = nullptr;
};

template<>
class BehaviorBlock<double> : public BlockBase {
public:
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition) : BlockBase(name, typeName) {
		this->baseDefinition = baseDefinition;
	};

	void SerializeToAmf(AMFArrayValue* bitStream) override {
		Game::logger->Log("BehaviorBlock", "Overloaded serialize for double!");
		BlockBase::SerializeToAmf(bitStream);
		AMFDoubleValue* serializedType = new AMFDoubleValue();
		serializedType->SetDoubleValue(GetCurrentValue());
		bitStream->InsertValue(typeName, serializedType);
	};

	double GetCurrentValue() { return currentValue; };
private:
	double currentValue;
	BlockDefinition* baseDefinition = nullptr;
};
