#include "GravityEngine.h"

void report_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    Game::logger->Log("GravityEngine", "%s\n", message);
}

GravityEngine::GravityEngine() {
    this->m_Delegate.error_callback = report_error;
}

GravityEngine::~GravityEngine() {
    
}

GravityScript* GravityEngine::Compile(const std::string& script) {
    auto* vm = gravity_vm_new(&m_Delegate);
    auto* compiler = gravity_compiler_create(&m_Delegate);
    auto* closure = gravity_compiler_run(compiler, script.c_str(), script.size(), 0, false, true);
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    if (!closure) {
        return nullptr;
    }

    gravity_vm_loadclosure(vm, closure);

    m_ScriptIdCounter += 1;
    GravityScript* gravityScript = new GravityScript(m_ScriptIdCounter, vm);

    return gravityScript;
}