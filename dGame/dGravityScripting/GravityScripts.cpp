#include "GravityScripts.h"

static bool SendChatMessage(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    gravity_value_t v1 = GET_VALUE(1);

    if (VALUE_ISA_STRING(v1)) {
        std::string message(VALUE_AS_STRING(v1)->s);
        ChatPackets::SendSystemMessage(UNASSIGNED_SYSTEM_ADDRESS, GeneralUtils::ASCIIToUTF16(message), true);
    }

    RETURN_VALUE(VALUE_FROM_BOOL(true), rindex);
}

GravityScript::GravityScript(uint32_t id, gravity_vm* vm) {
    m_VM = vm;
    m_Id = id;
    m_KnownFunctions = std::unordered_map<std::string, gravity_closure_t*>();

    for (const auto& item : toCheckFunctions) {
        auto func = gravity_vm_getvalue(m_VM, item.c_str(), (uint32_t)item.size());
        if (!VALUE_ISA_CLOSURE(func)) {
            continue;
        }

        m_KnownFunctions.insert({ item, VALUE_AS_CLOSURE(func) });
    }

    this->SetupGlobals();
}

GravityScript::~GravityScript() {
    gravity_vm_free(m_VM);
    delete m_VM;
}

void GravityScript::Call(std::string name, std::vector<gravity_value_t> args) {
    if (m_KnownFunctions.find(name) == m_KnownFunctions.end()) {
        return;
    }
    
    gravity_vm_runclosure(m_VM, m_KnownFunctions[name], VALUE_FROM_NULL, args.data(), args.size());

    gravity_vm_result(m_VM);
}

void GravityScript::SetupGlobals() {
    gravity_class_t *c = gravity_class_new_pair(m_VM, "Test", NULL, 0, 0);
    gravity_class_bind(c, "SendChatMessage", NEW_CLOSURE_VALUE(SendChatMessage));
    gravity_vm_setvalue(m_VM, "Test", VALUE_FROM_OBJECT(c));
}