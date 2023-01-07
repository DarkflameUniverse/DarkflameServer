#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

class BlockDefinition {
	std::string defaultValue;
	std::string minimumValue;
	std::string maximumValue;
};

/**
 * @brief Base class for all behavior block definitions.  Contains the block name and typeName.
 * This should be inherited by a templated class BlockDefinition which contains information about 
 * the actual block.
 */
class BlockBase {
public:
	BlockBase(std::string& name, std::string& typeName);

	/**
	 * @brief Serializes the Behavior to an AMFArrayValue
	 * 
	 * @param bitStream The AMFArrayValue to write this behavior to.
	 */
	virtual void SerializeToAmf(AMFArrayValue* bitStream);

	/**
	 * @brief Returns the name of this behavior block
	 */
	std::string& GetName() { return name; };

	/**
	 * @brief Returns the name of the type of this behavior block as a string
	 */
	std::string& GetTypeName() { return typeName; };
protected:
	std::string name;
	std::string typeName;
};

/**
 * @brief Class for storing behavior block definitions in memory. 
 * 
 * @tparam Type Type to base the block off of
 */
template<typename Type>
class BehaviorBlock : public BlockBase {
public:
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition) : BlockBase(name, typeName) {
		this->baseDefinition = baseDefinition;
	};

	void SerializeToAmf(AMFArrayValue* bitStream) override;

	Type GetCurrentValue() { return currentValue; };
private:
	Type currentValue;
	BlockDefinition* baseDefinition;
};

// Template includes
#include "BlockDefinition.tcc"

#endif  //!__BLOCKDEFINITION__H__
