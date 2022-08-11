#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"

#include "CDProvider.h"

CD_PROVIDER(BehaviorParameterProvider, size_t, float);

//! Constructor
CDBehaviorParameterTable::CDBehaviorParameterTable(void) {
	NEW_CD_PROVIDER(BehaviorParameterProvider, "BehaviorParameter", [](CppSQLite3Query& query) {
		size_t hash = 0;

		int32_t behaviorID = query.getIntField(0, -1);
		std::string parameterID = query.getStringField(1, "");
		
		GeneralUtils::hash_combine(hash, behaviorID);
		GeneralUtils::hash_combine(hash, parameterID);

		float value = query.getFloatField(2, -1.0f);

		return std::make_pair(hash, value);
	}, [](int32_t size) {
		return 40 * 1000 * 1000;
	}, false);
}

//! Destructor
CDBehaviorParameterTable::~CDBehaviorParameterTable(void) { }

//! Returns the table's name
std::string CDBehaviorParameterTable::GetName(void) const {
	return "BehaviorParameter";
}

float CDBehaviorParameterTable::GetEntry(const uint32_t behaviorID, const std::string& name, const float defaultValue) 
{
	size_t hash = 0;
	GeneralUtils::hash_combine(hash, behaviorID);
	GeneralUtils::hash_combine(hash, name);

	return BehaviorParameterProvider->GetEntry(hash, defaultValue);
}

void CDBehaviorParameterTable::LoadHost()
{
	BehaviorParameterProvider->LoadHost();
}
