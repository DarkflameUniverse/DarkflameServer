#pragma once

#include "dLua.h"

class LuaScript;

/**
 * A struct for wrapping a Position in Lua.
 */
struct lCommonTypes {
    static void RegisterClass(LuaScript* script);
};
