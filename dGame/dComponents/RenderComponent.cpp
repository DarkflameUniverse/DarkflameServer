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
#include "CDAnimationsTable.h"

std::unordered_map<int32_t, float> RenderComponent::m_DurationCache{};

RenderComponent::RenderComponent(Entity* parent, int32_t componentId): Component(parent) {
	m_Effects = std::vector<Effect*>();
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
					Game::logger->Log("RenderComponent", "bad animation group Id %s", groupId.c_str());
					continue;
				}
				m_animationGroupIds.push_back(groupIdInt);
				animationsTable->CacheAnimationGroup(groupIdInt);
			}
		}
	}
	result.finalize();
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

	GameMessages::SendPlayFXEffect(m_ParentEntity, effectId, effectType, name, secondary, priority, scale, serialize);

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
	GameMessages::SendStopFXEffect(m_ParentEntity, killImmediate, name);

	RemoveEffect(name);
}

std::vector<Effect*>& RenderComponent::GetEffects() {
	return m_Effects;
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
	if (returnlength == 0.0f) Game::logger->Log("RenderComponent", "WARNING: Unable to find animation %s for lot %i in any group.", animation.c_str(), self->GetLOT());
	return returnlength;
}
