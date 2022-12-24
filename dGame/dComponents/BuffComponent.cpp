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
#include "CDClientManager.h"

std::unordered_map<int32_t, std::vector<BuffParameter>> BuffComponent::m_Cache{};

BuffComponent::BuffComponent(Entity* parent) : Component(parent) {
}

BuffComponent::~BuffComponent() {
}

void BuffComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	// apply buffs from previous sessions that are stored in the charxml
	if (bIsInitialUpdate) {
		// buffs
		if (m_Buffs.empty()) {
			outBitStream->Write0(); // no buffs
		} else {
			outBitStream->Write1(); // we have some
			outBitStream->Write<uint32_t>(m_Buffs.size());

			for (const auto& buff : m_Buffs) {
				outBitStream->Write<uint32_t>(buff.first);
				outBitStream->Write(buff.second.time);
				outBitStream->Write(buff.second.cancelOnDeath);
				outBitStream->Write(buff.second.cancelOnZone);
				outBitStream->Write(buff.second.cancelOnDamaged);
				outBitStream->Write(buff.second.cancelOnRemoveBuff);
				outBitStream->Write(buff.second.cancelOnUi);
				outBitStream->Write(buff.second.cancelOnLogout);
				outBitStream->Write(buff.second.cancelOnUnequip);
				outBitStream->Write(buff.second.cancelOnDamageAbsDone);
				outBitStream->Write(buff.second.source != m_Parent->GetObjectID());
				outBitStream->Write(buff.second.applyOnTeammates);
				if (buff.second.source != m_Parent->GetObjectID()) outBitStream->Write(buff.second.source);
				outBitStream->Write(buff.second.refcount);
			}
		}

		// buff imunities
		if (m_BuffImmunities.empty()) {
			outBitStream->Write0(); // no buff immunities
		} else {
			outBitStream->Write1(); // we have some
			outBitStream->Write<uint32_t>(m_BuffImmunities.size());

			for (const auto& buffImmunity : m_BuffImmunities) {
				outBitStream->Write<uint32_t>(buffImmunity.first);
				outBitStream->Write(buffImmunity.second.time);
				outBitStream->Write(buffImmunity.second.cancelOnDeath);
				outBitStream->Write(buffImmunity.second.cancelOnZone);
				outBitStream->Write(buffImmunity.second.cancelOnDamaged);
				outBitStream->Write(buffImmunity.second.cancelOnRemoveBuff);
				outBitStream->Write(buffImmunity.second.cancelOnUi);
				outBitStream->Write(buffImmunity.second.cancelOnLogout);
				outBitStream->Write(buffImmunity.second.cancelOnUnequip);
				outBitStream->Write(buffImmunity.second.cancelOnDamageAbsDone);
				outBitStream->Write(buffImmunity.second.source != m_Parent->GetObjectID());
				outBitStream->Write(buffImmunity.second.applyOnTeammates);
				if (buffImmunity.second.source != m_Parent->GetObjectID()) outBitStream->Write(buffImmunity.second.source);
				outBitStream->Write(buffImmunity.second.refcount);
			}
		}
	} // there is no update serilization, only on initialization
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

void BuffComponent::ApplyBuff(Buff buff) {

	if (HasBuff(buff.id) && HasBuffImmunity(buff.id)) return;

	GameMessages::SendAddBuff(const_cast<LWOOBJID&>(m_Parent->GetObjectID()), buff);

	float tick = 0;
	float stacks = 0;
	int32_t behaviorID = 0;

	const auto& parameters = GetBuffParameters(buff.id);
	for (const auto& parameter : parameters) {
		if (parameter.name == "overtime") {
			auto* behaviorTemplateTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");

			behaviorID = behaviorTemplateTable->GetSkillByID(parameter.values[0]).behaviorID;
			stacks = static_cast<int32_t>(parameter.values[1]);
			tick = parameter.values[2];
			const auto unknown2 = parameter.values[3]; // TODO: figure this out it changed to all 1 in FT
		}
	}

	ApplyBuffEffect(buff.id);
	buff.tick = tick;
	buff.tickTime = tick;
	buff.stacks = stacks;
	buff.behaviorID = behaviorID;

	buff.refcount = 0;
	if (buff.addImmunity) {
		m_BuffImmunities.emplace(buff.id, buff);
	} else m_Buffs.emplace(buff.id, buff);
}

void BuffComponent::RemoveBuff(int32_t id, bool fromUnEquip, bool removeImmunity) {
	const auto& iter = m_Buffs.find(id);

	if (iter == m_Buffs.end()) {
		return;
	}

	GameMessages::SendRemoveBuff(m_Parent, fromUnEquip, removeImmunity, id);

	m_Buffs.erase(iter);

	RemoveBuffEffect(id);
}

bool BuffComponent::HasBuff(int32_t id) {
	return m_Buffs.find(id) != m_Buffs.end();
}

bool BuffComponent::HasBuffImmunity(int32_t id) {
	return m_BuffImmunities.find(id) != m_BuffImmunities.end();
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
			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();
			if (!controllablePhysicsComponent) return;
			const auto speed = parameter.value;
			controllablePhysicsComponent->AddSpeedboost(speed);
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
			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();
			if (!controllablePhysicsComponent) return;
			const auto speed = parameter.value;
			controllablePhysicsComponent->RemoveSpeedboost(speed);
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
		if (buff.second.cancelOnLogout || buff.second.cancelOnZone) continue;
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
