#ifndef __RENAMEMESSAGE__H__
#define __RENAMEMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class RenameMessage : public BehaviorMessageBase {
public:
	RenameMessage(AMFArrayValue* arguments);
	const std::string& GetName() { return name; };
private:
	std::string name;
};

#endif  //!__RENAMEMESSAGE__H__
