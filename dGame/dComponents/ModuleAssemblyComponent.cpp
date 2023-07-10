#include "ModuleAssemblyComponent.h"

#include "Entity.h"

ModuleAssemblyComponent::ModuleAssemblyComponent(Entity* parent) : Component(parent) {
	m_SubKey = LWOOBJID_EMPTY;
	m_UseOptionalParts = false;
}

void ModuleAssemblyComponent::SetAssemblyPartsLOTs(const std::u16string& value) {
	m_AssemblyPartsLOTs = value;
	std::replace(m_AssemblyPartsLOTs.begin(), m_AssemblyPartsLOTs.end(), u'+', u';');
	// doesn't matter if we push back a ; or a +. The client splits on either of them.
	// For congruency however, maintain one or the other.
	m_AssemblyPartsLOTs.push_back(u';');
}

void ModuleAssemblyComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (!bIsInitialUpdate) return;
	outBitStream->Write(bIsInitialUpdate);

	outBitStream->Write(m_SubKey != LWOOBJID_EMPTY);
	if (m_SubKey != LWOOBJID_EMPTY) outBitStream->Write(m_SubKey);

	outBitStream->Write(m_UseOptionalParts);

	outBitStream->Write<uint16_t>(m_AssemblyPartsLOTs.size());
	for (const char16_t character : m_AssemblyPartsLOTs) {
		outBitStream->Write(character);
	}
}
