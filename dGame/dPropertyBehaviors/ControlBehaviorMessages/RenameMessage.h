#ifndef __RENAMEMESSAGE__H__
#define __RENAMEMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

class RenameMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t behaviorID;
	std::string name;
};

#endif  //!__RENAMEMESSAGE__H__
