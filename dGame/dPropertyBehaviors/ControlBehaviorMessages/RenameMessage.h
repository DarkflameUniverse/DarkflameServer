#ifndef __RENAMEMESSAGE__H__
#define __RENAMEMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player renames this behavior
 */
class RenameMessage : public BehaviorMessageBase {
public:
	RenameMessage(const AMFArrayValue& arguments);
	[[nodiscard]] const std::string& GetName() const { return m_Name; };

private:
	std::string m_Name;
};

#endif  //!__RENAMEMESSAGE__H__
