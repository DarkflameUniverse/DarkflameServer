#ifndef CPPSCRIPTSOLD_H
#define CPPSCRIPTSOLD_H

#include <string>

class Entity;

namespace CppScripts {
	class Script;
};

namespace CppScriptsOld {
	CppScripts::Script* const GetScript(Entity* entity, const std::string& scriptName);
};

#endif  //!CPPSCRIPTSOLD_H
