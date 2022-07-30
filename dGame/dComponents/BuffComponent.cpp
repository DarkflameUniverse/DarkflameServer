#include "BuffComponent.h"
#include <BitStream.h>
#include "CDClientDatabase.h"
#include <stdexcept>
#include "DestroyableComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"

std::unordered_map<int32_t, std::vector<BuffParameter>> BuffComponent::m_Cache{};

BuffComponent::BuffComponent(Entity* parent) : Component(parent) {
}

BuffComponent::~BuffComponent() {
}

void BuffComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (!bIsInitialUpdate) return;
	if (m_Buffs.empty()) {
		outBitStream->Write0();
	} else {
		outBitStream->Write1();
		outBitStream->Write<uint32_t>(m_Buffs.size());

		for (const auto& buff : m_Buffs) {
			outBitStream->Write<uint32_t>(buff.first);
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();
			outBitStream->Write0();

			outBitStream->Write0();
			outBitStream->Write0();

			outBitStream->Write<uint32_t>(0);
		}
	}

	outBitStream->Write0();
}

void BuffComponent::Update(float deltaTime) {
	/**
	 * Loop through all buffs and apply deltaTime to ther time.
	 * If they have expired, remove the buff and break.
	 */
	for (auto& buff : m_Buffs) {
		// For damage buffs
		if (buff.second.tick != 0.0f && buff.second.stacks > 0) {
			buff.second.tickTime -= deltaTime;

			if (buff.second.tickTime <= 0.0f) {
				buff.second.tickTime = buff.second.tick;
				buff.second.stacks--;

				SkillComponent::HandleUnmanaged(buff.second.behaviorID, m_Parent->GetObjectID(), buff.second.source);
			}
		}

		// These are indefinate buffs, don't update them.
		if (buff.second.time == 0.0f) {
			continue;
		}

		buff.second.time -= deltaTime;

		if (buff.second.time <= 0.0f) {
			RemoveBuff(buff.first);

			break;
		}
	}
}

void BuffComponent::ApplyBuff(const int32_t id, const float duration, const LWOOBJID source, bool addImmunity,
	bool cancelOnDamaged, bool cancelOnDeath, bool cancelOnLogout, bool cancelOnRemoveBuff,
	bool cancelOnUi, bool cancelOnUnequip, bool cancelOnZone) {
	// Prevent buffs from stacking.
	if (HasBuff(id)) {
		return;
	}

	GameMessages::SendAddBuff(const_cast<LWOOBJID&>(m_Parent->GetObjectID()), source, (uint32_t)id,
		(uint32_t)duration * 1000, addImmunity, cancelOnDamaged, cancelOnDeath,
		cancelOnLogout, cancelOnRemoveBuff, cancelOnUi, cancelOnUnequip, cancelOnZone);

	float tick = 0;
	float stacks = 0;
	int32_t behaviorID = 0;

	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		if (parameter.name == "overtime") {
			auto* behaviorTemplateTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");

			behaviorID = behaviorTemplateTable->GetSkillByID(parameter.values[0]).behaviorID;
			stacks = static_cast<int32_t>(parameter.values[1]);
			tick = parameter.values[2];
			const auto unknown2 = parameter.values[3]; // Always 0
		}
	}

	ApplyBuffEffect(id);

	Buff buff;
	buff.id = id;
	buff.time = duration;
	buff.tick = tick;
	buff.tickTime = tick;
	buff.stacks = stacks;
	buff.source = source;
	buff.behaviorID = behaviorID;

	m_Buffs.emplace(id, buff);
}

void BuffComponent::RemoveBuff(int32_t id) {
	const auto& iter = m_Buffs.find(id);

	if (iter == m_Buffs.end()) {
		return;
	}

	m_Buffs.erase(iter);

	RemoveBuffEffect(id);
}

bool BuffComponent::HasBuff(int32_t id) {
	return m_Buffs.find(id) != m_Buffs.end();
}

void BuffComponent::ApplyBuffEffect(int32_t id) {
	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		if (parameter.name == "max_health") {
			const auto maxHealth = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxHealth(destroyable->GetMaxHealth() + maxHealth);
		} else if (parameter.name == "max_armor") {
			const auto maxArmor = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxArmor(destroyable->GetMaxArmor() + maxArmor);
		} else if (parameter.name == "max_imagination") {
			const auto maxImagination = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxImagination(destroyable->GetMaxImagination() + maxImagination);
		} else if (parameter.name == "speed") {
			const auto speed = parameter.value;

			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();

			if (controllablePhysicsComponent == nullptr) return;

			const auto current = controllablePhysicsComponent->GetSpeedMultiplier();

			controllablePhysicsComponent->SetSpeedMultiplier(current + ((speed - 500.0f) / 500.0f));

			EntityManager::Instance()->SerializeEntity(this->GetParent());
		}
	}
}

void BuffComponent::RemoveBuffEffect(int32_t id) {
	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		if (parameter.name == "max_health") {
			const auto maxHealth = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxHealth(destroyable->GetMaxHealth() - maxHealth);
		} else if (parameter.name == "max_armor") {
			const auto maxArmor = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxArmor(destroyable->GetMaxArmor() - maxArmor);
		} else if (parameter.name == "max_imagination") {
			const auto maxImagination = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxImagination(destroyable->GetMaxImagination() - maxImagination);
		} else if (parameter.name == "speed") {
			const auto speed = parameter.value;

			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();

			if (controllablePhysicsComponent == nullptr) return;

			const auto current = controllablePhysicsComponent->GetSpeedMultiplier();

			controllablePhysicsComponent->SetSpeedMultiplier(current - ((speed - 500.0f) / 500.0f));

			EntityManager::Instance()->SerializeEntity(this->GetParent());
		}
	}
}

void BuffComponent::RemoveAllBuffs() {
	for (const auto& buff : m_Buffs) {
		RemoveBuffEffect(buff.first);
	}

	m_Buffs.clear();
}

void BuffComponent::Reset() {
	RemoveAllBuffs();
}

void BuffComponent::ReApplyBuffs() {
	for (const auto& buff : m_Buffs) {
		ApplyBuffEffect(buff.first);
	}
}

Entity* BuffComponent::GetParent() const {
	return m_Parent;
}

void BuffComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	// Load buffs
	auto* dest = doc->FirstChildElement("obj")->FirstChildElement("dest");

	// Make sure we have a clean buff element.
	auto* buffElement = dest->FirstChildElement("buff");

	// Old character, no buffs to load
	if (buffElement == nullptr) {
		return;
	}

	auto* buffEntry = buffElement->FirstChildElement("b");

	while (buffEntry != nullptr) {
		int32_t id = buffEntry->IntAttribute("id");
		float t = buffEntry->FloatAttribute("t");
		float tk = buffEntry->FloatAttribute("tk");
		int32_t s = buffEntry->FloatAttribute("s");
		LWOOBJID sr = buffEntry->Int64Attribute("sr");
		int32_t b = buffEntry->IntAttribute("b");

		Buff buff;
		buff.id = id;
		buff.time = t;
		buff.tick = tk;
		buff.stacks = s;
		buff.source = sr;
		buff.behaviorID = b;

		m_Buffs.emplace(id, buff);

		buffEntry = buffEntry->NextSiblingElement("b");
	}
}

void BuffComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	// Save buffs
	auto* dest = doc->FirstChildElement("obj")->FirstChildElement("dest");

	// Make sure we have a clean buff element.
	auto* buffElement = dest->FirstChildElement("buff");

	if (buffElement == nullptr) {
		buffElement = doc->NewElement("buff");

		dest->LinkEndChild(buffElement);
	} else {
		buffElement->DeleteChildren();
	}

	for (const auto& buff : m_Buffs) {
		auto* buffEntry = doc->NewElement("b");

		buffEntry->SetAttribute("id", buff.first);
		buffEntry->SetAttribute("t", buff.second.time);
		buffEntry->SetAttribute("tk", buff.second.tick);
		buffEntry->SetAttribute("s", buff.second.stacks);
		buffEntry->SetAttribute("sr", buff.second.source);
		buffEntry->SetAttribute("b", buff.second.behaviorID);

		buffElement->LinkEndChild(buffEntry);
	}
}

const std::vector<BuffParameter>& BuffComponent::GetBuffParameters(int32_t buffId) {
	const auto& pair = m_Cache.find(buffId);

	if (pair != m_Cache.end()) {
		return pair->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT * FROM BuffParameters WHERE BuffID = ?;");
	query.bind(1, (int)buffId);

	auto result = query.execQuery();

	std::vector<BuffParameter> parameters{};

	while (!result.eof()) {
		BuffParameter param;

		param.buffId = buffId;
		param.name = result.getStringField(1);
		param.value = result.getFloatField(2);

		if (!result.fieldIsNull(3)) {
			std::istringstream stream(result.getStringField(3));
			std::string token;

			while (std::getline(stream, token, ',')) {
				try {
					const auto value = std::stof(token);

					param.values.push_back(value);
				} catch (std::invalid_argument& exception) {
					Game::logger->Log("BuffComponent", "Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
				}
			}
		}

		parameters.push_back(param);

		result.nextRow();
	}

	m_Cache.insert_or_assign(buffId, parameters);

	return m_Cache.find(buffId)->second;
}
