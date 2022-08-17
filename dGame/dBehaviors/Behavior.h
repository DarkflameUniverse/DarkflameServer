#pragma once

#include <map>
#include <string>
#include <vector>
#include <unordered_map>

#include "BitStream.h"
#include "BehaviorTemplates.h"
#include "dCommonVars.h"

struct BehaviorContext;
struct BehaviorBranchContext;
class CDBehaviorParameterTable;

class Behavior
{
public:
	/*
	 * Static
	 */
	static std::unordered_map<uint32_t, Behavior*> Cache;
	static CDBehaviorParameterTable* BehaviorParameterTable;

	static Behavior* GetBehavior(uint32_t behaviorId);

	static Behavior* CreateBehavior(uint32_t behaviorId);

	static BehaviorTemplates GetBehaviorTemplate(uint32_t behaviorId);

	/*
	 * Utilities
	 */

	void PlayFx(std::u16string type, LWOOBJID target, LWOOBJID secondary = LWOOBJID_EMPTY);

	/*
	 * Members
	 */

	uint32_t m_behaviorId;
	BehaviorTemplates m_templateId;
	uint32_t m_effectId;
	std::string* m_effectHandle = nullptr;
	std::unordered_map<std::string, std::string>* m_effectNames = nullptr;
	std::string* m_effectType = nullptr;

	/*
	 * Behavior parameters
	 */

	float GetFloat(const std::string& name, const float defaultValue = 0) const;

	bool GetBoolean(const std::string& name, const bool defaultValue = false) const;

	int32_t GetInt(const std::string& name, const int32_t defaultValue = 0) const;

	Behavior* GetAction(const std::string& name) const;

	Behavior* GetAction(float value) const;

	std::map<std::string, float> GetParameterNames() const;

	/*
	 * Virtual
	 */

	virtual void Load();

	// Player side
	virtual void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	virtual void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	virtual void UnCast(BehaviorContext* context, BehaviorBranchContext branch);

	virtual void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second);

	virtual void End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second);

	// Npc side
	virtual void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	virtual void SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	/*
	 * Creations/destruction
	 */

	explicit Behavior(uint32_t behaviorId);
	virtual ~Behavior();
};
