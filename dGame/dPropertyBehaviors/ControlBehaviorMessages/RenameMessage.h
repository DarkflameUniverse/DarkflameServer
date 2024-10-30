#ifndef RENAMEMESSAGE_H
#define RENAMEMESSAGE_H

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

#endif  //!RENAMEMESSAGE_H
