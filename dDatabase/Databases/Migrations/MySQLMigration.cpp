#include "MySQLMigration.h"

#include <istream>

#include "CDClientDatabase.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "dLogger.h"
#include "BinaryPathFinder.h"
#include "ZCompression.h"
#include "tinyxml2.h"

#include "../MySQL.h"

void MigrationRunner::RunMigrations(DatabaseBase* db) {
	auto database = dynamic_cast<MySQLDatabase*>(db);

	auto* stmt = database->CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
	stmt->execute();
	delete stmt;

	sql::SQLString finalSQL = "";
	bool runSd0Migrations = false;
	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "./migrations/dlu/mysql").string())) {
		auto migration = Migration::LoadMigration("dlu", "mysql", entry);

		if (migration.data.empty()) {
			continue;
		}

		stmt = database->CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		stmt->setString(1, migration.name.c_str());
		auto* res = stmt->executeQuery();
		bool doExit = res->next();
		delete res;
		delete stmt;
		if (doExit) continue;

		Game::logger->Log("MigrationRunner", "Running migration: %s", migration.name.c_str());
		if (migration.name == "dlu/5_brick_model_sd0.sql") {
			runSd0Migrations = true;
		} else {
			finalSQL.append(migration.data.c_str());
		}

		stmt = database->CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		stmt->setString(1, migration.name.c_str());
		stmt->execute();
		delete stmt;
	}

	if (finalSQL.empty() && !runSd0Migrations) {
		Game::logger->Log("MigrationRunner", "Server database is up to date.");
		return;
	}

	if (!finalSQL.empty()) {
		auto migration = GeneralUtils::SplitString(static_cast<std::string>(finalSQL), ';');
		std::unique_ptr<sql::Statement> simpleStatement(Database::CreateStmt());
		for (auto& query : migration) {
			try {
				if (query.empty()) continue;
				simpleStatement->execute(query.c_str());
			} catch (sql::SQLException& e) {
				Game::logger->Log("MigrationRunner", "Encountered error running migration: %s", e.what());
			}
		}
	}

	// Do this last on the off chance none of the other migrations have been run yet.
	if (runSd0Migrations) {
		uint32_t numberOfUpdatedModels = UpdateBrickByBrickModelsToSd0();
		Game::logger->Log("MasterServer", "%i models were updated from zlib to sd0.", numberOfUpdatedModels);
		
		uint32_t numberOfTruncatedModels = TruncateBrokenBrickByBrickXml();
		Game::logger->Log("MasterServer", "%i models were truncated from the database.", numberOfTruncatedModels);
	}
}


uint32_t TruncateBrokenBrickByBrickXml(MySQLDatabase* database) {
	uint32_t modelsTruncated{};
	auto modelsToTruncate = GetModelsFromDatabase(database);
	
	bool previousCommitValue = database->GetAutoCommit();
	database->SetAutoCommit(false);
	
	while (modelsToTruncate->next()) {
		std::unique_ptr<sql::PreparedStatement> ugcModelToDelete(database->CreatePreppedStmt("DELETE FROM ugc WHERE ugc.id = ?;"));
		std::unique_ptr<sql::PreparedStatement> pcModelToDelete(database->CreatePreppedStmt("DELETE FROM properties_contents WHERE ugc_id = ?;"));
		std::string completeUncompressedModel{};
		uint32_t chunkCount{};
		uint64_t modelId = modelsToTruncate->getInt(1);
		std::unique_ptr<sql::Blob> modelAsSd0(modelsToTruncate->getBlob(2));
		// Check that header is sd0 by checking for the sd0 magic.
		if (CheckSd0Magic(modelAsSd0.get())) {
			while (true) {
				uint32_t chunkSize{};
				modelAsSd0->read(reinterpret_cast<char*>(&chunkSize), sizeof(uint32_t)); // Extract chunk size from istream

				// Check if good here since if at the end of an sd0 file, this will have eof flagged.
				if (!modelAsSd0->good()) break;

				std::unique_ptr<uint8_t[]> compressedChunk(new uint8_t[chunkSize]);
				for (uint32_t i = 0; i < chunkSize; i++) {
					compressedChunk[i] = modelAsSd0->get();
				}

				// Ignore the valgrind warning about uninitialized values.  These are discarded later when we know the actual uncompressed size.
				std::unique_ptr<uint8_t[]> uncompressedChunk(new uint8_t[ZCompression::MAX_SD0_CHUNK_SIZE]);
				int32_t err{};
				int32_t actualUncompressedSize = ZCompression::Decompress(
					compressedChunk.get(), chunkSize, uncompressedChunk.get(), ZCompression::MAX_SD0_CHUNK_SIZE, err);

				if (actualUncompressedSize != -1) {
					uint32_t previousSize = completeUncompressedModel.size();
					completeUncompressedModel.append((char*)uncompressedChunk.get());
					completeUncompressedModel.resize(previousSize + actualUncompressedSize);
				} else {
					Game::logger->Log("BrickByBrickFix", "Failed to inflate chunk %i for model %llu.  Error: %i", chunkCount, modelId, err);
					break;
				}
				chunkCount++;
			}
			std::unique_ptr<tinyxml2::XMLDocument> document = std::make_unique<tinyxml2::XMLDocument>();
			if (!document) {
				Game::logger->Log("BrickByBrickFix", "Failed to initialize tinyxml document.  Aborting.");
				return 0;
			}

			if (!(document->Parse(completeUncompressedModel.c_str(), completeUncompressedModel.size()) == tinyxml2::XML_SUCCESS)) {
				if (completeUncompressedModel.find(
					"</LXFML>",
					completeUncompressedModel.length() >= 15 ? completeUncompressedModel.length() - 15 : 0) == std::string::npos
					) {
					Game::logger->Log("BrickByBrickFix",
						"Brick-by-brick model %llu will be deleted!", modelId);
					ugcModelToDelete->setInt64(1, modelsToTruncate->getInt64(1));
					pcModelToDelete->setInt64(1, modelsToTruncate->getInt64(1));
					ugcModelToDelete->execute();
					pcModelToDelete->execute();
					modelsTruncated++;
				}
			}
		} else {
			Game::logger->Log("BrickByBrickFix",
				"Brick-by-brick model %llu will be deleted!", modelId);
			ugcModelToDelete->setInt64(1, modelsToTruncate->getInt64(1));
			pcModelToDelete->setInt64(1, modelsToTruncate->getInt64(1));
			ugcModelToDelete->execute();
			pcModelToDelete->execute();
			modelsTruncated++;
		}
	}

	database->Commit();
	database->SetAutoCommit(previousCommitValue);
	return modelsTruncated;
}

/**
 * @brief Updates all current models in the database to have the Segmented Data 0 (SD0) format.
 * Any models that do not start with zlib and best compression magic will not be updated.
 *
 * @return The number of models updated to SD0
 */
uint32_t UpdateBrickByBrickModelsToSd0(MySQLDatabase* database) {
	uint32_t updatedModels = 0;
	auto modelsToUpdate = GetModelsFromDatabase();
	
	auto previousAutoCommitState = database->GetAutoCommit();
	database->SetAutoCommit(false);
	
	std::unique_ptr<sql::PreparedStatement> insertionStatement(database->CreatePreppedStmt("UPDATE ugc SET lxfml = ? WHERE id = ?;"));
	while (modelsToUpdate->next()) {
		int64_t modelId = modelsToUpdate->getInt64(1);
		std::unique_ptr<sql::Blob> oldLxfml(modelsToUpdate->getBlob(2));
		// Check if the stored blob starts with zlib magic (0x78 0xDA - best compression of zlib)
		// If it does, convert it to sd0.
		if (oldLxfml->get() == 0x78 && oldLxfml->get() == 0xDA) {

			// Get and save size of zlib compressed chunk.
			oldLxfml->seekg(0, std::ios::end);
			uint32_t oldLxfmlSize = static_cast<uint32_t>(oldLxfml->tellg());
			oldLxfml->seekg(0);

			// Allocate 9 extra bytes.  5 for sd0 magic, 4 for the only zlib compressed size.
			uint32_t oldLxfmlSizeWithHeader = oldLxfmlSize + 9;
			std::unique_ptr<char[]> sd0ConvertedModel(new char[oldLxfmlSizeWithHeader]);

			WriteSd0Magic(sd0ConvertedModel.get(), oldLxfmlSize);
			for (uint32_t i = 9; i < oldLxfmlSizeWithHeader; i++) {
				sd0ConvertedModel.get()[i] = oldLxfml->get();
			}

			std::string outputString(sd0ConvertedModel.get(), oldLxfmlSizeWithHeader);
			std::istringstream outputStringStream(outputString);

			insertionStatement->setBlob(1, static_cast<std::istream*>(&outputStringStream));
			insertionStatement->setInt64(2, modelId);
			try {
				insertionStatement->executeUpdate();
				Game::logger->Log("BrickByBrickFix", "Updated model %i to sd0", modelId);
				updatedModels++;
			} catch (sql::SQLException exception) {
				Game::logger->Log(
					"BrickByBrickFix",
					"Failed to update model %i.  This model should be inspected manually to see why."
					"The database error is %s", modelId, exception.what());
			}
		}
	}
	
	database->Commit();
	database->SetAutoCommit(previousAutoCommitState);
	return updatedModels;
}

std::unique_ptr<sql::ResultSet> GetModelsFromDatabase(MySQLDatabase* database) {
	std::unique_ptr<sql::PreparedStatement> modelsRawDataQuery(database->CreatePreppedStmt("SELECT id, lxfml FROM ugc;"));
	return std::unique_ptr<sql::ResultSet>(modelsRawDataQuery->executeQuery());
}

/**
 * @brief Writes sd0 magic at the front of a char*
 *
 * @param input the char* to write at the front of
 * @param chunkSize The size of the first chunk to write the size of
 */
void WriteSd0Magic(char* input, uint32_t chunkSize) {
	input[0] = 's';
	input[1] = 'd';
	input[2] = '0';
	input[3] = 0x01;
	input[4] = 0xFF;
	*reinterpret_cast<uint32_t*>(input + 5) = chunkSize; // Write the integer to the character array
}

bool CheckSd0Magic(sql::Blob* streamToCheck) {
	return streamToCheck->get() == 's' && streamToCheck->get() == 'd' && streamToCheck->get() == '0' && streamToCheck->get() == 0x01 && streamToCheck->get() == 0xFF;
}
