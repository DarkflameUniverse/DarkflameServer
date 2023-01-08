#ifndef __BEHAVIORBLOCK__H__
#define __BEHAVIORBLOCK__H__

#include <string>

class AMFArrayValue;
class BlockDefinition;

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
	BehaviorBlock(std::string& name, std::string& typeName, BlockDefinition* baseDefinition);

	void SerializeToAmf(AMFArrayValue* bitStream) override;

	Type GetCurrentValue() { return currentValue; };
private:
	Type currentValue;
	Type defaultValue;
	BlockDefinition* baseDefinition;
};

// Template includes
#include "BehaviorBlock.tcc"

#endif  //!__BEHAVIORBLOCK__H__
