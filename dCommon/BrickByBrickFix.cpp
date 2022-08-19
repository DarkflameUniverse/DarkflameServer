#include <sstream>
#include <memory>

#include "BrickByBrickFix.h"
#include "Database.h"
#include "dLogger.h"
#include "Game.h"

//! Forward declarations

std::unique_ptr<sql::ResultSet> GetModelsFromDatabase();
void WriteSd0Magic(char* input);

uint32_t BrickByBrickFix::TruncateBrokenBrickByBrickXml() {
	auto modelsToTruncate = GetModelsFromDatabase();
	if (modelsToTruncate->next()) {
		// Decompress with zlib and attempt to convert to xml.  If that fails this model is broken.
	}
	return 0;
}

uint32_t BrickByBrickFix::UpdateBrickByBrickModelsToSd0() {
	uint32_t updatedModels = 0;
	auto modelsToUpdate = GetModelsFromDatabase();
	auto previousAutoCommitState = Database::GetAutoCommit();
	Database::SetAutoCommit(false);
	std::unique_ptr<sql::PreparedStatement> insertionStatement(Database::CreatePreppedStmt("UPDATE ugc SET lxfml = ? WHERE id = ?;"));
	while (modelsToUpdate->next()) {
		uint32_t modelId = modelsToUpdate->getInt(1);
		auto oldLxfml = modelsToUpdate->getBlob(2);
		// Check if the stored blob starts with zlib magic (0x78).  If it does, convert it to sd0.
		if (oldLxfml->get() == 0x78) {
			Game::logger->Log("BrickByBrickFix", "Updating model %i", modelId);

			// Get and save size of zlib compressed chunk.
			oldLxfml->seekg(0, std::ios::end);
			uint32_t oldLxfmlSize = static_cast<uint32_t>(oldLxfml->tellg());
			oldLxfml->seekg(0);

			// Allocate 9 extra bytes.  5 for sd0 magic, 4 for the only zlib compressed size.
			uint32_t oldLxfmlSizeWithHeader = oldLxfmlSize + 9;
			char* sd0ConvertedModel = static_cast<char*>(std::malloc(oldLxfmlSizeWithHeader));

			WriteSd0Magic(sd0ConvertedModel);
			// zlib compressed chunk size
			sd0ConvertedModel[5] = static_cast<uint8_t>(oldLxfmlSize);
			sd0ConvertedModel[6] = static_cast<uint8_t>(oldLxfmlSize >> 8U);
			sd0ConvertedModel[7] = static_cast<uint8_t>(oldLxfmlSize >> 16U);
			sd0ConvertedModel[8] = static_cast<uint8_t>(oldLxfmlSize >> 24U);
			for (uint32_t i = 9; i < oldLxfmlSizeWithHeader; i++) {
				sd0ConvertedModel[i] = oldLxfml->get();
			}

			std::string outputString(sd0ConvertedModel, oldLxfmlSizeWithHeader);
			std::istringstream outputStringStream(outputString);

			insertionStatement->setBlob(1, static_cast<std::istream*>(&outputStringStream));
			insertionStatement->setInt(2, modelId);
			try {
				insertionStatement->executeUpdate();
				Game::logger->Log("BrickByBrickFix", "Updated model %i", modelId);
				updatedModels++;
			} catch (sql::SQLException exception) {
				Game::logger->Log(
					"BrickByBrickFix",
					"Failed to update model %i.  This model should be inspected manually to see why."
					"The database error is %s", modelId, exception.what());
			}
			free(sd0ConvertedModel);
		}
	}
	Database::Commit();
	Database::SetAutoCommit(previousAutoCommitState);
	return updatedModels;
}

std::unique_ptr<sql::ResultSet> GetModelsFromDatabase() {
	std::unique_ptr<sql::PreparedStatement> modelsRawDataQuery(Database::CreatePreppedStmt("SELECT id, lxfml FROM ugc;"));
	return std::unique_ptr<sql::ResultSet>(modelsRawDataQuery->executeQuery());
}

void WriteSd0Magic(char* input) {
	input[0] = 's';
	input[1] = 'd';
	input[2] = '0';
	input[3] = 0x01;
	input[4] = 0xFF;
}
