#include "CDSceneTableTable.h"

namespace CDSceneTableTable {
	Table entries;

	void LoadValuesFromDatabase() {
		// Get the data from the database
		auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM SceneTable");
		while (!tableData.eof()) {
			CDSceneTable entry;
			entry.sceneID = tableData.getIntField("sceneID", -1);
			entry.sceneName = tableData.getStringField("sceneName", "");

			entries[entry.sceneID] = entry;
			tableData.nextRow();
		}
	}

	//! Queries the table with a sceneID to find.
	const CDSceneTable* Query(uint32_t sceneID) {
		const auto& iter = entries.find(sceneID);
		if (iter != entries.end()) {
			return &iter->second;
		}

		return nullptr;
	}
}
