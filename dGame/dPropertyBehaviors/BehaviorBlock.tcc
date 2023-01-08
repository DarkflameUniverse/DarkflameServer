#ifndef __BEHAVIORBLOCK__H__

#error Do not include this file directly.  Include BehaviorBlock.h instead.

#endif //! __BEHAVIORBLOCK__H__

#include <string>

#include "AMFFormat.h"
#include "BitStream.h"
#include "BehaviorBlock.h"
#include "BlockDefinition.h"
#include "Game.h"
#include "dLogger.h"

template<>
class BehaviorBlock<std::string> : public BlockBase {
public:
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition) : BlockBase(name, typeName) {
		this->baseDefinition = baseDefinition;
		if (baseDefinition) this->defaultValue = baseDefinition->defaultValue;
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
	std::string defaultValue;
	BlockDefinition* baseDefinition = nullptr;
};

template<>
class BehaviorBlock<double> : public BlockBase {
public:
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition) : BlockBase(name, typeName) {
		this->baseDefinition = baseDefinition;
		if (baseDefinition) this->defaultValue = std::stod(baseDefinition->defaultValue);
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
	double defaultValue;
	BlockDefinition* baseDefinition = nullptr;
};
