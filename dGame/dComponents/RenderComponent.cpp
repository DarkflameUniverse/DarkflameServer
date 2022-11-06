#include "RenderComponent.h"

#include <sstream>
#include <string>
#include <iomanip>

#include "Entity.h"
#include "PacketUtils.h"

#include "CDClientManager.h"
#include "GameMessages.h"
#include "Game.h"
#include "dLogger.h"

std::unordered_map<int32_t, float> RenderComponent::m_DurationCache{};

RenderComponent::RenderComponent(Entity* parent) : Component(parent) {
	m_Effects = std::vector<Effect*>();

	return;

	/*
	auto* table = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	const auto entry = table->GetByIDAndType(parent->GetLOT(), COMPONENT_TYPE_RENDER);

	std::stringstream query;

	query << "SELECT effect1, effect2, effect3, effect4, effect5, effect6 FROM RenderComponent WHERE id = " << std::to_string(entry) << ";";

	auto result = CDClientDatabase::ExecuteQuery(query.str());

	if (result.eof())
	{
		return;
	}

	for (auto i = 0; i < 6; ++i)
	{
		if (result.fieldIsNull(i))
		{
			continue;
		}

		const auto id = result.getIntField(i);

		if (id <= 0)
		{
			continue;
		}

		query.clear();

		query << "SELECT effectType, effectName FROM BehaviorEffect WHERE effectID = " << std::to_string(id) << ";";

		auto effectResult = CDClientDatabase::ExecuteQuery(query.str());

		while (!effectResult.eof())
		{
			const auto type = effectResult.fieldIsNull(0) ? "" : std::string(effectResult.getStringField(0));

			const auto name = effectResult.fieldIsNull(1) ? "" : std::string(effectResult.getStringField(1));

			auto* effect = new Effect();

			effect->name = name;
			effect->type = GeneralUtils::ASCIIToUTF16(type);
			effect->scale = 1;
			effect->effectID = id;
			effect->secondary = LWOOBJID_EMPTY;

			m_Effects.push_back(effect);

			effectResult.nextRow();
		}
	}

	result.finalize();
	*/
}

RenderComponent::~RenderComponent() {
	for (Effect* eff : m_Effects) {
		if (eff) {
			delete eff;
			eff = nullptr;
		}
	}

	m_Effects.clear();
}

void RenderComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (!bIsInitialUpdate) return;

	outBitStream->Write<uint32_t>(m_Effects.size());

	for (Effect* eff : m_Effects) {
		// Check that the effect is non-null
		assert(eff);

		outBitStream->Write<uint8_t>(eff->name.size());
		for (const auto& value : eff->name)
			outBitStream->Write<uint8_t>(value);

		outBitStream->Write(eff->effectID);

		outBitStream->Write<uint8_t>(eff->type.size());
		for (const auto& value : eff->type)
			outBitStream->Write<uint16_t>(value);

		outBitStream->Write<float_t>(eff->scale);
		outBitStream->Write<int64_t>(eff->secondary);
	}
}

Effect* RenderComponent::AddEffect(const int32_t effectId, const std::string& name, const std::u16string& type) {
	auto* eff = new Effect();

	eff->effectID = effectId;

	eff->name = name;

	eff->type = type;

	m_Effects.push_back(eff);

	return eff;
}

void RenderComponent::RemoveEffect(const std::string& name) {
	uint32_t index = -1;

	for (auto i = 0u; i < m_Effects.size(); ++i) {
		auto* eff = m_Effects[i];

		if (eff->name == name) {
			index = i;

			delete eff;

			break;
		}
	}

	if (index == -1) {
		return;
	}

	m_Effects.erase(m_Effects.begin() + index);
}

void RenderComponent::Update(const float deltaTime) {
	std::vector<Effect*> dead;

	for (auto* effect : m_Effects) {
		if (effect->time == 0) {
			continue; // Skip persistent effects
		}

		const auto result = effect->time - deltaTime;

		if (result <= 0) {
			dead.push_back(effect);

			continue;
		}

		effect->time = result;
	}

	for (auto* effect : dead) {
		//        StopEffect(effect->name);
	}
}

void RenderComponent::PlayEffect(const int32_t effectId, const std::u16string& effectType, const std::string& name, const LWOOBJID secondary, const float priority, const float scale, const bool serialize) {
	RemoveEffect(name);

	GameMessages::SendPlayFXEffect(m_Parent, effectId, effectType, name, secondary, priority, scale, serialize);

	auto* effect = AddEffect(effectId, name, effectType);

	const auto& pair = m_DurationCache.find(effectId);

	if (pair != m_DurationCache.end()) {
		effect->time = pair->second;

		return;
	}

	const std::string effectType_str = GeneralUtils::UTF16ToWTF8(effectType);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT animation_length FROM Animations WHERE animation_type IN (SELECT animationName FROM BehaviorEffect WHERE effectID = ? AND effectType = ?);");
	query.bind(1, effectId);
	query.bind(2, effectType_str.c_str());

	auto result = query.execQuery();

	if (result.eof() || result.fieldIsNull(0)) {
		result.finalize();

		m_DurationCache[effectId] = 0;

		effect->time = 0; // Persistent effect

		return;
	}

	effect->time = static_cast<float>(result.getFloatField(0));

	result.finalize();

	m_DurationCache[effectId] = effect->time;
}

void RenderComponent::StopEffect(const std::string& name, const bool killImmediate) {
	GameMessages::SendStopFXEffect(m_Parent, killImmediate, name);

	RemoveEffect(name);
}

std::vector<Effect*>& RenderComponent::GetEffects() {
	return m_Effects;
}
