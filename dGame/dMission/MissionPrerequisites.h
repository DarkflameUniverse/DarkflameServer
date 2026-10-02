#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Mission.h"

/**
 * Utility class for checking whether or not a mission can be accepted using its prerequisites
 */
class MissionPrerequisites final
{
public:
	/**
	 * Checks whether or not the mission identified by the specified ID can be accepted based on the mission inventory passed.
	 * Also performs checks for daily missions (e.g. if the time out is valid).
	 * @param missionId the mission ID to check prerequisites for
	 * @param missions the mission inventory to check the prerequisites against
	 * @return whether or not the mission identified by the specified ID can be accepted
	 */
	static bool CanAccept(uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions);

	/**
	 * Checks the prerequisites (Missions.prereqMissionID) of a mission, as the client
	 * (LWOMissionComponent::getMissionState 0x00cd7400): an expression is evaluated by EvaluateExpression with
	 * IsOperandMet for each operand; text without any of !&^;,|(): is one mission ID, met when 0 or once ever completed.
	 * @param missionId the mission ID to check prerequisites for
	 * @param missions the mission inventory to check the prerequisites against
	 * @return whether or not the prerequisites of the mission identified by the specified ID are met
	 */
	static bool CheckPrerequisites(uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions);

	/**
	 * The client's state bit mask for a mission (LWOMissionComponent::getMissionState 0x00cd7400): 8 once ever
	 * completed; in progress, its state (with 8 if completed before); else 1 when never completed (or repeatable) and
	 * the prerequisites are met; 0 for no mission.
	 */
	static int32_t GetClientMissionState(uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions);

	/**
	 * One operand of a prerequisite expression (sub_00c90a80): "id" is met once the mission was ever completed;
	 * "id:state" when the client's state mask of that mission is state (a mission naming itself counts as state 0).
	 */
	static bool IsOperandMet(std::string_view operand, uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions);

	/**
	 * The client's boolean expression evaluator (sub_00a32980): read left to right, no precedence; & , ; and, | or,
	 * ^ xor, ! ~ negate the next operand, ( ) group. And/or skip their right side once the result is known; an operand
	 * or group with no operator before it (other than the first) is ignored.
	 */
	static bool EvaluateExpression(std::string_view expression, const std::function<bool(std::string_view)>& isMet);

private:
	/**
	 * Cache of the prerequisite text of each mission
	 */
	static std::unordered_map<uint32_t, std::string> expressions;
};
