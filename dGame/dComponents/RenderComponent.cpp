#include "RenderComponent.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <iomanip>

#include "Entity.h"

#include "CDClientManager.h"
#include "GameMessages.h"
#include "Game.h"
#include "Logger.h"
#include "CDAnimationsTable.h"

std::unordered_map<int32_t, float> RenderComponent::m_DurationCache{};

RenderComponent::RenderComponent(Entity* const parentEntity, const int32_t componentId) : Component{ parentEntity } {
	m_LastAnimationName = "";
	if (componentId == -1) return;

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM RenderComponent WHERE id = ?;");
	query.bind(1, componentId);
	auto result = query.execQuery();

	if (!result.eof()) {
		auto animationGroupIDs = std::string(result.getStringField("animationGroupIDs", ""));
		if (!animationGroupIDs.empty()) {
			auto* animationsTable = CDClientManager::Instance().GetTable<CDAnimationsTable>();
			auto groupIdsSplit = GeneralUtils::SplitString(animationGroupIDs, ',');
			for (auto& groupId : groupIdsSplit) {
				int32_t groupIdInt;
				if (!GeneralUtils::TryParse(groupId, groupIdInt)) {
					LOG("bad animation group Id %s", groupId.c_str());
					continue;
				}
				m_animationGroupIds.push_back(groupIdInt);
				animationsTable->CacheAnimationGroup(groupIdInt);
			}
		}
	}
	result.finalize();
}

void RenderComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	if (!bIsInitialUpdate) return;

	outBitStream->Write<uint32_t>(m_Effects.size());

	for (auto& eff : m_Effects) {
		outBitStream->Write<uint8_t>(eff.name.size());
		// if there is no name, then we don't write anything else
		if (eff.name.empty()) continue;

		for (const auto& value : eff.name) outBitStream->Write<uint8_t>(value);

		outBitStream->Write(eff.effectID);

		outBitStream->Write<uint8_t>(eff.type.size());
		for (const auto& value : eff.type) outBitStream->Write<uint16_t>(value);

		outBitStream->Write<float_t>(eff.priority);
		outBitStream->Write<int64_t>(eff.secondary);
	}
}

Effect& RenderComponent::AddEffect(const int32_t effectId, const std::string& name, const std::u16string& type, const float priority) {
	return m_Effects.emplace_back(Effect{ effectId, name, type, priority });
}

void RenderComponent::RemoveEffect(const std::string& name) {
	if (m_Effects.size() < 1) return; // Don't try to delete if the container is already empty

	const auto effectToRemove = std::ranges::find_if(m_Effects, [&name](auto&& effect) { return effect.name == name; });
	if (effectToRemove == m_Effects.end()) return; // Return early if effect is not present

	const auto lastEffect = m_Effects.rbegin();
	*effectToRemove = std::move(*lastEffect); // Move-overwrite
	m_Effects.pop_back();
}

void RenderComponent::Update(const float deltaTime) {	
	for (auto& effect : m_Effects) {
		if (effect.time == 0) continue; // Skip persistent effects

		const auto result = effect.time - deltaTime;
		if (result <= 0) continue;

		effect.time = result;
	}
}

void RenderComponent::PlayEffect(const int32_t effectId, const std::u16string& effectType, const std::string& name, const LWOOBJID secondary, const float priority, const float scale, const bool serialize) {
	RemoveEffect(name);

	GameMessages::SendPlayFXEffect(m_Parent, effectId, effectType, name, secondary, priority, scale, serialize);

	auto& effect = AddEffect(effectId, name, effectType, priority);

	const auto& pair = m_DurationCache.find(effectId);

	if (pair != m_DurationCache.end()) {
		effect.time = pair->second;

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

		effect.time = 0; // Persistent effect

		return;
	}

	effect.time = static_cast<float>(result.getFloatField(0));

	result.finalize();

	m_DurationCache[effectId] = effect.time;
}

void RenderComponent::StopEffect(const std::string& name, const bool killImmediate) {
	GameMessages::SendStopFXEffect(m_Parent, killImmediate, name);

	RemoveEffect(name);
}

float RenderComponent::PlayAnimation(Entity* self, const std::u16string& animation, float priority, float scale) {
	if (!self) return 0.0f;
	return RenderComponent::PlayAnimation(self, GeneralUtils::UTF16ToWTF8(animation), priority, scale);
}

float RenderComponent::PlayAnimation(Entity* self, const std::string& animation, float priority, float scale) {
	if (!self) return 0.0f;
	return RenderComponent::DoAnimation(self, animation, true, priority, scale);
}

float RenderComponent::GetAnimationTime(Entity* self, const std::u16string& animation) {
	if (!self) return 0.0f;
	return RenderComponent::GetAnimationTime(self, GeneralUtils::UTF16ToWTF8(animation));
}

float RenderComponent::GetAnimationTime(Entity* self, const std::string& animation) {
	if (!self) return 0.0f;
	return RenderComponent::DoAnimation(self, animation, false);
}


float RenderComponent::DoAnimation(Entity* self, const std::string& animation, bool sendAnimation, float priority, float scale) {
	float returnlength = 0.0f;
	if (!self) return returnlength;
	auto* renderComponent = self->GetComponent<RenderComponent>();
	if (!renderComponent) return returnlength;

	auto* animationsTable = CDClientManager::Instance().GetTable<CDAnimationsTable>();
	for (auto& groupId : renderComponent->m_animationGroupIds) {
		auto animationGroup = animationsTable->GetAnimation(animation, renderComponent->GetLastAnimationName(), groupId);
		if (animationGroup.FoundData()) {
			auto data = animationGroup.Data();
			renderComponent->SetLastAnimationName(data.animation_name);
			returnlength = data.animation_length;
		}
	}
	if (sendAnimation) GameMessages::SendPlayAnimation(self, GeneralUtils::ASCIIToUTF16(animation), priority, scale);
	if (returnlength == 0.0f) LOG("WARNING: Unable to find animation %s for lot %i in any group.", animation.c_str(), self->GetLOT());
	return returnlength;
}
