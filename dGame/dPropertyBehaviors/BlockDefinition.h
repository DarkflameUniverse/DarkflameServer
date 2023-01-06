#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

#include "Game.h"
#include "dLogger.h"

/**
 * @brief Base class for all behavior block definitions.  Contains the block name and typeName.
 * This should be inherited by a templated class BlockDefinition which contains information about 
 * the actual block.
 */
class BlockBase {
public:
	BlockBase(std::string& name, std::string& typeName) {
		this->name = name;
		this->typeName = typeName;
	};
	std::string name;
	std::string typeName;
	virtual void Serialize() { Game::logger->Log("BlockDefinition", "not overloaded serialize"); };
};

/**
 * @brief Class for storing behavior block definitions in memory. 
 * 
 * @tparam Primitive type to base the block off of
 */
template<typename Type>
class BlockDefinition : public BlockBase {
public:
	BlockDefinition(std::string& name, std::string& typeName, Type defaultValue = Type(), Type minimumValue = Type(), Type maximumValue = Type()) : BlockBase(name, typeName) {
		this->defaultValue = defaultValue;
		this->minimumValue = minimumValue;
		this->maximumValue = maximumValue;
	}

	void Serialize() override {
		Game::logger->Log("BlockDefinition", "Overloaded serialize!");
	};

	Type GetDefaultValue() { return defaultValue; };
	Type GetMinimumValue() { return minimumValue; };
	Type GetMaximumValue() { return maximumValue; };
// private:
	Type defaultValue;
	Type minimumValue;
	Type maximumValue;
};

#endif  //!__BLOCKDEFINITION__H__
