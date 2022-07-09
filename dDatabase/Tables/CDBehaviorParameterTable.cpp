#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <functional>
#include <utility>
#include <chrono>

typedef size_t    KeyType;
typedef float  MappedType;
typedef std::pair<const KeyType, MappedType> ValueType;

typedef boost::interprocess::allocator<ValueType, boost::interprocess::managed_shared_memory::segment_manager> ShmemAllocator;

typedef boost::interprocess::map<KeyType, MappedType, std::less<KeyType>, ShmemAllocator> CDBehaviorParameterMap;

CDBehaviorParameterMap *table = nullptr;

CppSQLite3Statement preparedStatementSingle;
CppSQLite3Statement preparedStatementMultiple;

//#define CACHE_ALL_BEHAVIORS

//! Constructor
CDBehaviorParameterTable::CDBehaviorParameterTable(void) {

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

	// Search for specific parameter
	try {
		boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "CDBehaviorParameterTable");

		// If the segment manager is not null, then the segment exists
		// Get the table from the segment
		ShmemAllocator alloc_inst (segment.get_segment_manager());

		boost::interprocess::offset_ptr<CDBehaviorParameterMap> m = segment.find<CDBehaviorParameterMap>("CDBehaviorParameter").first;

		if (m == nullptr) {
			Game::logger->Log("CDBehaviorParameterTable", "CDBehaviorParameter segment is nullptr!\n");

			return defaultValue;
		}

		auto it = m->find(hash);

		if (it != m->end()) {
			return it->second;
		}

		return defaultValue;
	}
	catch (std::exception &e) {
		Game::logger->Log("CDBehaviorParameterTable", "Failed to find entry for behaviorID: %d, parameterID: %s\n%e\n", behaviorID, name.c_str(), e.what());

		return defaultValue;
	}
}

void CDBehaviorParameterTable::CreateSharedMap()
{
	boost::interprocess::shared_memory_object::remove("CDBehaviorParameterTable");

	Game::logger->Log("CDBehaviorParameterTable", "Failed to open or create shared memory segment, creating...\n");

	// If the segment manager is null, then the segment does not exist
	// Create the segment
	boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only, "CDBehaviorParameterTable", 40 * 1000 * 1000);

	ShmemAllocator alloc_inst (segment.get_segment_manager());

	table = segment.construct<CDBehaviorParameterMap>("CDBehaviorParameter")      //object name
								(std::less<KeyType>() //first  ctor parameter
								,alloc_inst);     //second ctor parameter

	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorParameter");

	size_t hash = 0;

	while (!tableData.eof()) {
		hash = 0;

		int32_t behaviorID = tableData.getIntField(0, -1);
		std::string parameterID = tableData.getStringField(1, "");
		
		GeneralUtils::hash_combine(hash, behaviorID);
		GeneralUtils::hash_combine(hash, parameterID);

		float value = tableData.getFloatField(2, -1.0f);

		table->insert(ValueType(hash, value));

		tableData.nextRow();
	}

	tableData.finalize();
}
