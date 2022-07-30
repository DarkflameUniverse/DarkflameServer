#include "ModuleAssemblyComponent.h"

ModuleAssemblyComponent::ModuleAssemblyComponent(Entity* parent) : Component(parent) {
	m_SubKey = LWOOBJID_EMPTY;
	m_UseOptionalParts = false;
	m_AssemblyPartsLOTs = u"";
}

ModuleAssemblyComponent::~ModuleAssemblyComponent() {

}

void ModuleAssemblyComponent::SetSubKey(LWOOBJID value) {
	m_SubKey = value;
}

LWOOBJID ModuleAssemblyComponent::GetSubKey() const {
	return m_SubKey;
}

void ModuleAssemblyComponent::SetUseOptionalParts(bool value) {
	m_UseOptionalParts = value;
}

bool ModuleAssemblyComponent::GetUseOptionalParts() const {
	return m_UseOptionalParts;
}

void ModuleAssemblyComponent::SetAssemblyPartsLOTs(const std::u16string& value) {
	std::u16string val{};

	val.reserve(value.size() + 1);

	for (auto character : value) {
		if (character == '+') character = ';';

		val.push_back(character);
	}

	val.push_back(';');

	m_AssemblyPartsLOTs = val;
}

const std::u16string& ModuleAssemblyComponent::GetAssemblyPartsLOTs() const {
	return m_AssemblyPartsLOTs;
}

void ModuleAssemblyComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate) {
		outBitStream->Write1();

		outBitStream->Write(m_SubKey != LWOOBJID_EMPTY);
		if (m_SubKey != LWOOBJID_EMPTY) {
			outBitStream->Write(m_SubKey);
		}

		outBitStream->Write(m_UseOptionalParts);

		outBitStream->Write(static_cast<uint16_t>(m_AssemblyPartsLOTs.size()));
		for (char16_t character : m_AssemblyPartsLOTs) {
			outBitStream->Write(character);
		}
	}
}

void ModuleAssemblyComponent::Update(float deltaTime) {

}

