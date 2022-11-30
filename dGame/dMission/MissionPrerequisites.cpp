#include "MissionPrerequisites.h"

#include <sstream>
#include <ctime>

#include "CDClientManager.h"
#include "dLogger.h"


PrerequisiteExpression::PrerequisiteExpression(const std::string& str) {
	std::stringstream a;
	std::stringstream b;
	std::stringstream s;

	auto bor = false;

	auto sub = false;

	auto done = false;

	for (auto i = 0u; i < str.size(); ++i) {
		if (done) {
			break;
		}

		const auto character = str[i];

		switch (character) {
		case '|':
			bor = true;
			b << str.substr(i + 1);
			done = true;
			break;
		case ' ':
		case ')':
			break;
		case ',':
		case '&':
		case '(':
			b << str.substr(i + 1);
			done = true;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (sub) {
				s << character;
			} else {
				a << character;
			}
			break;
		case ':':
			sub = true;
			break;
		default:
			break;
		}
	}

	this->m_or = bor;

	const auto aString = a.str();

	if (!aString.empty()) {
		this->a = std::stoul(a.str());
	} else {
		this->a = 0;
	}

	const auto subString = s.str();

	if (!subString.empty()) {
		this->sub = std::stoul(s.str());
	} else {
		this->sub = 0;
	}

	const auto bString = b.str();

	if (!bString.empty()) {
		this->b = new PrerequisiteExpression(bString);
	} else {
		this->b = nullptr;
	}
}


bool PrerequisiteExpression::Execute(const std::unordered_map<uint32_t, Mission*>& missions) const {
	auto a = this->a == 0;

	auto b = this->b == nullptr;

	if (!a) {
		const auto index = missions.find(this->a);

		if (index != missions.end()) {
			const auto* mission = index->second;

			if (this->sub != 0) {
				// Special case for one Wisp Lee repeatable mission.
				a = mission->GetClientInfo().id == 1883 ?
					mission->GetMissionState() == static_cast<MissionState>(this->sub) :
					mission->GetMissionState() >= static_cast<MissionState>(this->sub);
			} else if (mission->IsComplete()) {
				a = true;
			}
		}
	}

	if (!b) {
		b = this->b->Execute(missions);
	}

	if (this->m_or) {
		return a || b;
	}

	return a && b;
}


PrerequisiteExpression::~PrerequisiteExpression() {
	delete b;
}


bool MissionPrerequisites::CanAccept(const uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	const auto& missionIndex = missions.find(missionId);

	if (missionIndex != missions.end()) {
		auto* mission = missionIndex->second;
		const auto& info = mission->GetClientInfo();

		if (info.repeatable) {
			const auto prerequisitesMet = CheckPrerequisites(missionId, missions);

			// Checked by client
			const time_t time = std::time(nullptr);
			const time_t lock = mission->GetTimestamp() + info.cooldownTime * 60;

			// If there's no time limit, just check the prerequisites, otherwise make sure both conditions are met
			return (info.cooldownTime == -1 ? prerequisitesMet : (lock - time < 0)) && prerequisitesMet;
		}

		// Mission is already accepted and cannot be repeatedly accepted
		return false;
	}

	// Mission is not yet accepted, check the prerequisites
	return CheckPrerequisites(missionId, missions);
}

bool MissionPrerequisites::CheckPrerequisites(uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	const auto& index = expressions.find(missionId);
	if (index != expressions.end()) {
		return index->second->Execute(missions);
	}

	auto* missionsTable = CDClientManager::Instance()->GetTable<CDMissionsTable>("Missions");
	const auto missionEntries = missionsTable->Query([=](const CDMissions& entry) {
		return entry.id == static_cast<int>(missionId);
		});

	if (missionEntries.empty())
		return false;

	auto* expression = new PrerequisiteExpression(missionEntries[0].prereqMissionID);
	expressions.insert_or_assign(missionId, expression);

	return expression->Execute(missions);
}

std::unordered_map<uint32_t, PrerequisiteExpression*> MissionPrerequisites::expressions = {};
