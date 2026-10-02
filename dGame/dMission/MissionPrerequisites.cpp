#include "MissionPrerequisites.h"

#include <cstdlib>
#include <ctime>
#include <limits>

#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "MissionComponent.h"
#include "eMissionState.h"

namespace {
	// The token classes of the client's expression reader (sub_00a32720)
	enum class eToken { End, Open, Close, And, Or, Xor, Not, Word, Space, None };

	eToken Classify(const std::string_view text, const size_t pos) {
		if (pos >= text.size()) return eToken::End;
		switch (text[pos]) {
		case '\0': return eToken::End;
		case '\t': case '\n': case '\r': case ' ': return eToken::Space;
		case '!': case '~': return eToken::Not;
		case '&': case ',': case ';': return eToken::And;
		case '(': return eToken::Open;
		case ')': return eToken::Close;
		case '^': return eToken::Xor;
		case '|': return eToken::Or;
		default: return eToken::Word;
		}
	}

	// The next token from pos (sub_00a328d0), skipping spaces; a word is read whole
	eToken NextToken(const std::string_view text, size_t& pos, std::string_view& word) {
		auto token = eToken::Space;
		while (token == eToken::Space) token = Classify(text, pos++);
		if (token != eToken::Word) return token;

		const auto start = pos - 1;
		while (Classify(text, pos) == eToken::Word) pos++;
		word = text.substr(start, pos - start);
		return token;
	}

	// Moves past the ')' closing the group just opened (sub_00a32840); false when the text ends first
	bool SkipGroup(const std::string_view text, size_t& pos) {
		int32_t depth = 1;
		while (depth > 0) {
			const auto token = Classify(text, pos++);
			if (token == eToken::End) return false;
			if (token == eToken::Open) depth++;
			else if (token == eToken::Close) depth--;
		}
		return true;
	}

	// sub_00a32980: one level, up to its ')' or the end of the text
	bool Evaluate(const std::string_view text, size_t& pos, const std::function<bool(std::string_view)>& isMet) {
		bool result = false;
		auto op = eToken::Space; // the first operand sets the result
		bool positive = true;
		bool seenOperand = false;

		const auto apply = [&](const std::function<bool()>& value) {
			switch (op) {
			case eToken::Or: if (!result) result = value(); break;
			case eToken::And: if (result) result = value(); break;
			case eToken::Xor: result = result ^ value(); break;
			case eToken::Space: result = value(); break;
			default: break; // no operator: ignored
			}
		};

		while (true) {
			std::string_view word;
			const auto token = NextToken(text, pos, word);
			switch (token) {
			case eToken::End:
			case eToken::Close:
				return result;
			case eToken::Open: {
				auto inner = pos;
				apply([&] { return Evaluate(text, inner, isMet) == positive; });
				seenOperand = true;
				positive = true;
				op = eToken::None;
				if (!SkipGroup(text, pos)) return result;
				break;
			}
			case eToken::And:
			case eToken::Or:
			case eToken::Xor:
				if (seenOperand) op = token;
				break;
			case eToken::Not:
				positive = !positive;
				break;
			case eToken::Word:
				apply([&] { return isMet(word) == positive; });
				seenOperand = true;
				positive = true;
				op = eToken::None;
				break;
			default:
				break;
			}
		}
	}

	// wcstol as the client reads an ID: leading digits, 0 when there are none
	int32_t ReadInt(const std::string_view text) {
		const std::string copy(text);
		const auto value = std::strtol(copy.c_str(), nullptr, 10);
		if (value > std::numeric_limits<int32_t>::max()) return std::numeric_limits<int32_t>::max();
		if (value < std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::min();
		return static_cast<int32_t>(value);
	}

	// LWOMissionComponent::IsMissionDone: the mission is in the completed list
	bool EverCompleted(const int32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
		if (missionId <= 0) return false;
		const auto it = missions.find(static_cast<uint32_t>(missionId));
		return it != missions.end() && it->second && (it->second->GetCompletions() > 0 || it->second->IsComplete());
	}
}

bool MissionPrerequisites::EvaluateExpression(const std::string_view expression, const std::function<bool(std::string_view)>& isMet) {
	size_t pos = 0;
	return Evaluate(expression, pos, isMet);
}

bool MissionPrerequisites::IsOperandMet(const std::string_view operand, const uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	constexpr auto COMPLETE = static_cast<int32_t>(eMissionState::COMPLETE);

	const auto colon = operand.find(':');
	const auto id = ReadInt(operand.substr(0, colon));
	const auto wanted = colon == std::string_view::npos ? COMPLETE : ReadInt(operand.substr(colon + 1));

	int32_t state = 0;
	if (wanted == COMPLETE) {
		state = EverCompleted(id, missions) ? COMPLETE : 0;
	} else if (id == static_cast<int64_t>(missionId)) {
		state = static_cast<int32_t>(eMissionState::REWARDING); // the client logs a mission naming itself
	} else if (id > 0) {
		state = GetClientMissionState(static_cast<uint32_t>(id), missions);
	}
	return state == wanted;
}

int32_t MissionPrerequisites::GetClientMissionState(const uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	// A mission ID below 1 is no mission
	if (missionId == 0 || missionId > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) return 0;

	const auto it = missions.find(missionId);
	const auto* const mission = it != missions.end() ? it->second : nullptr;
	const bool done = mission && (mission->GetCompletions() > 0 || mission->IsComplete());
	int32_t state = done ? static_cast<int32_t>(eMissionState::COMPLETE) : 0;

	// In progress: its own state
	if (mission && !mission->IsComplete()) return state | static_cast<int32_t>(mission->GetMissionState());

	CDMissions info{};
	if (!Mission::IsValidMission(missionId, info)) return 0;

	if ((!done || info.repeatable) && CheckPrerequisites(missionId, missions)) {
		state |= static_cast<int32_t>(eMissionState::AVAILABLE);
	}

	return state;
}

bool MissionPrerequisites::CanAccept(const uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	const auto& missionIndex = missions.find(missionId);

	if (missionIndex != missions.end()) {
		auto* mission = missionIndex->second;
		const auto& info = mission->GetClientInfo();

		if (info.repeatable) {
			// The client's cooldown (sub_00cd7770): whole minutes since the last completion, none when <= 0
			const auto now = static_cast<int64_t>(std::time(nullptr));
			if (MissionComponent::IsCooldownRunning(mission->GetTimestamp(), info.cooldownTime, now)) return false;

			return CheckPrerequisites(missionId, missions);
		}

		// Mission is already accepted and cannot be repeatedly accepted
		return false;
	}

	// Mission is not yet accepted, check the prerequisites
	return CheckPrerequisites(missionId, missions);
}

bool MissionPrerequisites::CheckPrerequisites(const uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions) {
	auto index = expressions.find(missionId);
	if (index == expressions.end()) {
		CDMissions info{};
		if (!Mission::IsValidMission(missionId, info)) return false;
		index = expressions.insert_or_assign(missionId, info.prereqMissionID).first;
	}

	const std::string_view text = index->second;
	if (text.empty()) return true;

	// Without any of the client's operator characters (L"!&^;,|():") the text is one mission ID (wcstol), met when 0
	// or completed
	if (text.find_first_of("!&^;,|():") == std::string_view::npos) {
		const auto id = ReadInt(text);
		return id == 0 || EverCompleted(id, missions);
	}

	return EvaluateExpression(text, [&](const std::string_view operand) { return IsOperandMet(operand, missionId, missions); });
}

std::unordered_map<uint32_t, std::string> MissionPrerequisites::expressions = {};
