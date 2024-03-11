#include "StripUiPosition.h"

#include "Amf3.h"

StripUiPosition::StripUiPosition(const AMFArrayValue& arguments, const std::string_view uiKeyName) {
	const auto* const uiArray = arguments.GetArray(uiKeyName);
	if (!uiArray) return;

	const auto* const xPositionValue = uiArray->Get<double>("x");
	if (!xPositionValue) return;

	const auto* const yPositionValue = uiArray->Get<double>("y");
	if (!yPositionValue) return;

	m_YPosition = yPositionValue->GetValue();
	m_XPosition = xPositionValue->GetValue();
}

void StripUiPosition::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* const uiArgs = args.InsertArray("ui");
	uiArgs->Insert("x", m_XPosition);
	uiArgs->Insert("y", m_YPosition);
}
