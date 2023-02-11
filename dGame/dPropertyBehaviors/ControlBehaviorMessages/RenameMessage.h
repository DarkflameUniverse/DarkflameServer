#ifndef __RENAMEMESSAGE__H__
#define __RENAMEMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class RenameMessage : public BehaviorMessageBase {
public:
	RenameMessage(AMFArrayValue* arguments);
	uint32_t GetBehaviorID() { return behaviorID; };
	std::string& GetName() { return name; };
private:
	uint32_t behaviorID;
	std::string name;
};

#endif  //!__RENAMEMESSAGE__H__
