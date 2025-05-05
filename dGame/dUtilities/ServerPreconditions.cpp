#include "ServerPreconditions.h"

#include "tinyxml2.h"

using namespace ServerPreconditions;

namespace {
	std::unordered_map<LOT, std::vector<std::pair<bool, PreconditionExpression>>> m_Preconditions;

	std::unordered_map<LWOOBJID, LWOOBJID> m_SoloActors;

	std::unordered_map<LWOOBJID, std::unordered_set<LWOOBJID>> m_ExcludeForPlayer;
}

void ServerPreconditions::LoadPreconditions(std::string file) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(file.c_str());

	tinyxml2::XMLElement* root = doc.FirstChildElement("Preconditions");
	if (!root) {
		return;
	}

	for (tinyxml2::XMLElement* element = root->FirstChildElement("Entity"); element; element = element->NextSiblingElement("Entity")) {
		LOT lot = element->UnsignedAttribute("lot");
		std::vector<std::pair<bool, PreconditionExpression>> preconditions;

		for (tinyxml2::XMLElement* precondition = element->FirstChildElement("Precondition"); precondition; precondition = precondition->NextSiblingElement("Precondition")) {
			const auto condition = Preconditions::CreateExpression(precondition->GetText());

			int64_t inverted;

			if (precondition->QueryInt64Attribute("not", &inverted) == tinyxml2::XML_SUCCESS) {
				preconditions.push_back(std::make_pair(inverted > 0, condition));
			}
			else {
				preconditions.push_back(std::make_pair(false, condition));
			}
		}

		m_Preconditions[lot] = preconditions;
	}
}

bool ServerPreconditions::CheckPreconditions(Entity* target, Entity* entity) {
	if (IsExcludedFor(entity->GetObjectID(), target->GetObjectID())) {
		return false;
	}

	if (IsSoloActor(target->GetObjectID())) {
		return IsActingFor(target->GetObjectID(), entity->GetObjectID());
	}

	if (m_Preconditions.find(target->GetLOT()) == m_Preconditions.end()) {
		return true;
	}

	for (const auto& [inverse, precondition] : m_Preconditions[target->GetLOT()]) {
		if (precondition.Check(entity) == inverse) {
			return false;
		}
	}

	return true;
}

bool ServerPreconditions::IsSoloActor(LWOOBJID actor) {
	return m_SoloActors.find(actor) != m_SoloActors.end();
}

bool ServerPreconditions::IsActingFor(LWOOBJID actor, LWOOBJID target) {
	return m_SoloActors.find(actor) != m_SoloActors.end() && m_SoloActors[actor] == target;
}

void ServerPreconditions::AddSoloActor(LWOOBJID actor, LWOOBJID target) {
	m_SoloActors[actor] = target;
}

void ServerPreconditions::RemoveSoloActor(LWOOBJID actor) {
	m_SoloActors.erase(actor);
}

void ServerPreconditions::AddExcludeFor(LWOOBJID player, LWOOBJID target) {
	const auto& it = m_ExcludeForPlayer.find(player);

	if (it == m_ExcludeForPlayer.end()) {
		m_ExcludeForPlayer[player] = std::unordered_set<LWOOBJID>();
	}

	m_ExcludeForPlayer[player].insert(target);
}

void ServerPreconditions::RemoveExcludeFor(LWOOBJID player, LWOOBJID target) {
	const auto& it = m_ExcludeForPlayer.find(player);

	if (it == m_ExcludeForPlayer.end()) {
		return;
	}

	m_ExcludeForPlayer[player].erase(target);

	if (m_ExcludeForPlayer[player].empty()) {
		m_ExcludeForPlayer.erase(player);
	}
}

bool ServerPreconditions::IsExcludedFor(LWOOBJID player, LWOOBJID target) {
	const auto& it = m_ExcludeForPlayer.find(player);

	if (it == m_ExcludeForPlayer.end()) {
		return false;
	}

	return it->second.find(target) != it->second.end();
}
