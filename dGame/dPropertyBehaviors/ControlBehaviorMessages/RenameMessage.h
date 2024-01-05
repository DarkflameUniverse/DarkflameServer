#ifndef __RENAMEMESSAGE__H__
#define __RENAMEMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player renames this behavior
 * 
 */
class RenameMessage : public BehaviorMessageBase {
public:
	RenameMessage(AMFArrayValue* arguments);
	const std::string& GetName() const { return name; };
private:
	std::string name;
};

#endif  //!__RENAMEMESSAGE__H__
