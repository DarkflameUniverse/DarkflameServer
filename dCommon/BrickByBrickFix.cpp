#include "BrickByBrickFix.h"

#include <memory>
#include <iostream>
#include <sstream>

#include "tinyxml2.h"

#include "Database.h"
#include "Game.h"
#include "ZCompression.h"
#include "Logger.h"

//! Forward declarations

void WriteSd0Magic(char* input, uint32_t chunkSize);
bool CheckSd0Magic(std::istream& streamToCheck);

/**
 * @brief Truncates all models with broken data from the database.
 *
 * @return The number of models deleted
 */
uint32_t BrickByBrickFix::TruncateBrokenBrickByBrickXml() {
	uint32_t modelsTruncated{};
	auto modelsToTruncate = Database::Get()->GetAllUgcModels();
	bool previousCommitValue = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);
	for (auto& model : modelsToTruncate) {
		std::string completeUncompressedModel{};
		uint32_t chunkCount{};
		// Check that header is sd0 by checking for the sd0 magic.
		if (CheckSd0Magic(model.lxfmlData)) {
			while (true) {
				uint32_t chunkSize{};
				model.lxfmlData.read(reinterpret_cast<char*>(&chunkSize), sizeof(uint32_t)); // Extract chunk size from istream

				// Check if good here since if at the end of an sd0 file, this will have eof flagged.
				if (!model.lxfmlData.good()) break;

				std::unique_ptr<uint8_t[]> compressedChunk(new uint8_t[chunkSize]);
				for (uint32_t i = 0; i < chunkSize; i++) {
					compressedChunk[i] = model.lxfmlData.get();
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
					LOG("Failed to inflate chunk %i for model %llu.  Error: %i", chunkCount, model.id, err);
					break;
				}
				chunkCount++;
			}
			std::unique_ptr<tinyxml2::XMLDocument> document = std::make_unique<tinyxml2::XMLDocument>();
			if (!document) {
				LOG("Failed to initialize tinyxml document.  Aborting.");
				return 0;
			}

			if (!(document->Parse(completeUncompressedModel.c_str(), completeUncompressedModel.size()) == tinyxml2::XML_SUCCESS)) {
				if (completeUncompressedModel.find(
					"</LXFML>",
					completeUncompressedModel.length() >= 15 ? completeUncompressedModel.length() - 15 : 0) == std::string::npos
					) {
					LOG("Brick-by-brick model %llu will be deleted!", model.id);
					Database::Get()->DeleteUgcModelData(model.id);
					modelsTruncated++;
				}
			}
		} else {
			LOG("Brick-by-brick model %llu will be deleted!", model.id);
			Database::Get()->DeleteUgcModelData(model.id);
			modelsTruncated++;
		}
	}

	Database::Get()->Commit();
	Database::Get()->SetAutoCommit(previousCommitValue);
	return modelsTruncated;
}

/**
 * @brief Updates all current models in the database to have the Segmented Data 0 (SD0) format.
 * Any models that do not start with zlib and best compression magic will not be updated.
 *
 * @return The number of models updated to SD0
 */
uint32_t BrickByBrickFix::UpdateBrickByBrickModelsToSd0() {
	uint32_t updatedModels = 0;
	auto modelsToUpdate = Database::Get()->GetAllUgcModels();
	auto previousAutoCommitState = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);
	for (auto& model : modelsToUpdate) {
		// Check if the stored blob starts with zlib magic (0x78 0xDA - best compression of zlib)
		// If it does, convert it to sd0.
		if (model.lxfmlData.get() == 0x78 && model.lxfmlData.get() == 0xDA) {
			// Get and save size of zlib compressed chunk.
			model.lxfmlData.seekg(0, std::ios::end);
			uint32_t oldLxfmlSize = static_cast<uint32_t>(model.lxfmlData.tellg());
			model.lxfmlData.seekg(0);

			// Allocate 9 extra bytes.  5 for sd0 magic, 4 for the only zlib compressed size.
			uint32_t oldLxfmlSizeWithHeader = oldLxfmlSize + 9;
			std::unique_ptr<char[]> sd0ConvertedModel(new char[oldLxfmlSizeWithHeader]);

			WriteSd0Magic(sd0ConvertedModel.get(), oldLxfmlSize);
			for (uint32_t i = 9; i < oldLxfmlSizeWithHeader; i++) {
				sd0ConvertedModel.get()[i] = model.lxfmlData.get();
			}

			std::string outputString(sd0ConvertedModel.get(), oldLxfmlSizeWithHeader);
			std::istringstream outputStringStream(outputString);

			try {
				Database::Get()->UpdateUgcModelData(model.id, outputStringStream);
				LOG("Updated model %i to sd0", model.id);
				updatedModels++;
			} catch (sql::SQLException exception) {
				LOG("Failed to update model %i.  This model should be inspected manually to see why."
					"The database error is %s", model.id, exception.what());
			}
		}
	}
	Database::Get()->Commit();
	Database::Get()->SetAutoCommit(previousAutoCommitState);
	return updatedModels;
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

bool CheckSd0Magic(std::istream& streamToCheck) {
	return streamToCheck.get() == 's' && streamToCheck.get() == 'd' && streamToCheck.get() == '0' && streamToCheck.get() == 0x01 && streamToCheck.get() == 0xFF;
}
