#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

// Minimum and maximum values are always ints, but used for different things in different contexts
struct BlockDefinition {
	std::string defaultValue = "";
	float minimumValue = 0;		// Always zero!
	float maximumValue = 0;
};

#endif  //!__BLOCKDEFINITION__H__
