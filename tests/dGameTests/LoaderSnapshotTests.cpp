// Tool (task 51): writes every LOT's component list and a hash of its construction bytes to $DLU_LOADER_SNAPSHOT, built from the real CDServer.sqlite in
// $DLU_LOADER_CDSERVER, so the loader rework can be compared with the old loader. Skipped without both.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "eReplicaPacketType.h"

class LoaderSnapshotTest : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(LoaderSnapshotTest, EveryLot) {
	const char* const out = std::getenv("DLU_LOADER_SNAPSHOT");
	const char* const db = std::getenv("DLU_LOADER_CDSERVER");
	if (!out || !db) GTEST_SKIP() << "DLU_LOADER_SNAPSHOT / DLU_LOADER_CDSERVER not set";
	CDClientDatabase::Connect(db);
	CDClientManager::LoadValuesFromDatabase();

	std::vector<int32_t> lots;
	auto query = CDClientDatabase::ExecuteQuery("SELECT id FROM Objects ORDER BY id;");
	while (!query.eof()) {
		lots.push_back(query.getIntField(0));
		query.nextRow();
	}
	std::ofstream file(out);
	LWOOBJID id = 288300744895990000;
	for (const auto lot : lots) {
		if (lot == 1) continue;
		info.lot = lot;
		Entity entity(id++, info);
		entity.Initialize();
		std::vector<int32_t> types;
		for (const auto& [type, component] : entity.GetComponents()) types.push_back(static_cast<int32_t>(type));
		std::sort(types.begin(), types.end());
		file << lot << ":";
		for (const auto type : types) file << " " << type;
		if (auto* const destroyable = entity.GetComponent<DestroyableComponent>()) {
			file << " | hp " << destroyable->GetMaxHealth() << " im " << destroyable->GetMaxImagination() << " sm " << destroyable->GetIsSmashable() << " f";
			for (const auto faction : destroyable->GetFactionIDs()) file << " " << faction;
		}
		// The construction's component bytes, as a 64-bit FNV-1a hash
		RakNet::BitStream bytes;
		entity.WriteComponents(bytes, eReplicaPacketType::CONSTRUCTION);
		uint64_t hash = 14695981039346656037ull;
		for (uint32_t i = 0; i < bytes.GetNumberOfBytesUsed(); i++) hash = (hash ^ bytes.GetData()[i]) * 1099511628211ull;
		file << " | bits " << bytes.GetNumberOfBitsUsed() << " " << std::hex << hash << std::dec;
		file << "\n";
	}
}
