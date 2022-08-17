#pragma once
#include <vector>
#include <string>
#include <map>

#include "Mission.h"

/**
 * An expression that checks if a mission may be accepted or not
 */
class PrerequisiteExpression final
{
	bool m_or;
	uint32_t a;
	uint32_t sub;
	PrerequisiteExpression* b;

public:
	/**
	 * Executes the prerequisite, checking its contents and returning whether or not the mission may be accepted
	 * @param missions the list of missions to check the prerequisites against (f.e. whether they're completed)
	 * @return whether or not all the prerequisites are met
	 */
	bool Execute(const std::unordered_map<uint32_t, Mission*>& missions) const;

	explicit PrerequisiteExpression(const std::string& str);
	~PrerequisiteExpression();
};

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
private:

	/**
	 * Cache of all the executed prerequisites
	 */
	static std::unordered_map<uint32_t, PrerequisiteExpression*> expressions;

	/**
	 * Checks the prerequisites for a mission
	 * @param missionId the mission ID to check prerequisites for
	 * @param missions the mission inventory to check the prerequisites against
	 * @return whether or not the mission identified by the specified ID can be accepted
	 */
	static bool CheckPrerequisites(uint32_t missionId, const std::unordered_map<uint32_t, Mission*>& missions);
};
