#pragma once

#include <cstdint>

#include "dCommonVars.h"
#include "Singleton.h"

#include "GravityScripts.h"

class GravityEngine : public Singleton<GravityEngine> {
public:
    GravityEngine();
    ~GravityEngine();

    GravityScript* Compile(const std::string& script);
private:
    gravity_delegate_t m_Delegate;
    uint32_t m_ScriptIdCounter = 0;
};