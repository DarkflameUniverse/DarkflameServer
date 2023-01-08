#include "BehaviorBlock.h"

#include "AMFFormat.h"

BlockBase::BlockBase(std::string& name, std::string& typeName) {
	this->name = name;
	this->typeName = typeName;
}

void BlockBase::SerializeToAmf(AMFArrayValue* bitStream) {
	Game::logger->Log("BlockDefinition", "Base class serialize");

	AMFStringValue* serializedType = new AMFStringValue();
	serializedType->SetStringValue(name);
	bitStream->InsertValue("Type", serializedType);

	AMFStringValue* serializedCallbackId = new AMFStringValue();
	serializedCallbackId->SetStringValue("");
	bitStream->InsertValue("__callbackID__", serializedCallbackId);
}
