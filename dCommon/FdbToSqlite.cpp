#include "FdbToSqlite.h"

#include <map>
#include <fstream>
#include <cassert>
#include <iomanip>

#include "BinaryIO.h"
#include "CDClientDatabase.h"
#include "GeneralUtils.h"
#include "Game.h"
#include "Logger.h"
#include "AssetManager.h"

#include "eSqliteDataType.h"

std::map<eSqliteDataType, std::string> FdbToSqlite::Convert::m_SqliteType = {
			{ eSqliteDataType::NONE, "none"},
			{ eSqliteDataType::INT32, "int32"},
			{ eSqliteDataType::REAL, "real"},
			{ eSqliteDataType::TEXT_4, "text_4"},
			{ eSqliteDataType::INT_BOOL, "int_bool"},
			{ eSqliteDataType::INT64, "int64"},
			{ eSqliteDataType::TEXT_8, "text_8"}
};

FdbToSqlite::Convert::Convert(std::string binaryOutPath) {
	this->m_BinaryOutPath = binaryOutPath;
}

bool FdbToSqlite::Convert::ConvertDatabase(AssetStream& buffer) {
	if (m_ConversionStarted) return false;

	this->m_ConversionStarted = true;
	try {
		CDClientDatabase::Connect(m_BinaryOutPath + "/CDServer.sqlite");

		CDClientDatabase::ExecuteQuery("BEGIN TRANSACTION;");

		int32_t numberOfTables = ReadInt32(buffer);
		ReadTables(numberOfTables, buffer);

		CDClientDatabase::ExecuteQuery("COMMIT;");
	} catch (CppSQLite3Exception& e) {
		LOG("Encountered error %s converting FDB to SQLite", e.errorMessage());
		return false;
	}

	return true;
}

int32_t FdbToSqlite::Convert::ReadInt32(std::istream& cdClientBuffer) {
	int32_t nextInt{};
	BinaryIO::BinaryRead(cdClientBuffer, nextInt);
	return nextInt;
}

int64_t FdbToSqlite::Convert::ReadInt64(std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int64_t value{};
	BinaryIO::BinaryRead(cdClientBuffer, value);

	cdClientBuffer.seekg(prevPosition);
	return value;
}

std::string FdbToSqlite::Convert::ReadString(std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	auto readString = BinaryIO::ReadString(cdClientBuffer);

	cdClientBuffer.seekg(prevPosition);
	return readString;
}

int32_t FdbToSqlite::Convert::SeekPointer(std::istream& cdClientBuffer) {
	int32_t position{};
	BinaryIO::BinaryRead(cdClientBuffer, position);
	int32_t prevPosition = cdClientBuffer.tellg();
	cdClientBuffer.seekg(position);
	return prevPosition;
}

std::string FdbToSqlite::Convert::ReadColumnHeader(std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int32_t numberOfColumns = ReadInt32(cdClientBuffer);
	std::string tableName = ReadString(cdClientBuffer);

	auto columns = ReadColumns(numberOfColumns, cdClientBuffer);
	std::string newTable = "CREATE TABLE IF NOT EXISTS '" + tableName + "' (" + columns + ");";
	CDClientDatabase::ExecuteDML(newTable);

	cdClientBuffer.seekg(prevPosition);

	return tableName;
}

void FdbToSqlite::Convert::ReadTables(int32_t& numberOfTables, std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	for (int32_t i = 0; i < numberOfTables; i++) {
		auto columnHeader = ReadColumnHeader(cdClientBuffer);
		ReadRowHeader(columnHeader, cdClientBuffer);
	}

	cdClientBuffer.seekg(prevPosition);
}

std::string FdbToSqlite::Convert::ReadColumns(int32_t& numberOfColumns, std::istream& cdClientBuffer) {
	std::stringstream columnsToCreate;
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	std::string name{};
	eSqliteDataType dataType{};
	for (int32_t i = 0; i < numberOfColumns; i++) {
		if (i != 0) columnsToCreate << ", ";
		dataType = static_cast<eSqliteDataType>(ReadInt32(cdClientBuffer));
		name = ReadString(cdClientBuffer);
		columnsToCreate << "'" << name << "' " << FdbToSqlite::Convert::m_SqliteType[dataType];
	}

	cdClientBuffer.seekg(prevPosition);
	return columnsToCreate.str();
}

void FdbToSqlite::Convert::ReadRowHeader(std::string& tableName, std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int32_t numberOfAllocatedRows = ReadInt32(cdClientBuffer);
	if (numberOfAllocatedRows != 0) assert((numberOfAllocatedRows & (numberOfAllocatedRows - 1)) == 0);  // assert power of 2 allocation size
	ReadRows(numberOfAllocatedRows, tableName, cdClientBuffer);

	cdClientBuffer.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRows(int32_t& numberOfAllocatedRows, std::string& tableName, std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int32_t rowid = 0;
	for (int32_t row = 0; row < numberOfAllocatedRows; row++) {
		int32_t rowPointer = ReadInt32(cdClientBuffer);
		if (rowPointer == -1) rowid++;
		else ReadRow(rowPointer, tableName, cdClientBuffer);
	}

	cdClientBuffer.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRow(int32_t& position, std::string& tableName, std::istream& cdClientBuffer) {
	int32_t prevPosition = cdClientBuffer.tellg();
	cdClientBuffer.seekg(position);

	while (true) {
		ReadRowInfo(tableName, cdClientBuffer);
		int32_t linked = ReadInt32(cdClientBuffer);
		if (linked == -1) break;
		cdClientBuffer.seekg(linked);
	}

	cdClientBuffer.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRowInfo(std::string& tableName, std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int32_t numberOfColumns = ReadInt32(cdClientBuffer);
	ReadRowValues(numberOfColumns, tableName, cdClientBuffer);

	cdClientBuffer.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRowValues(int32_t& numberOfColumns, std::string& tableName, std::istream& cdClientBuffer) {
	int32_t prevPosition = SeekPointer(cdClientBuffer);

	int32_t emptyValue{};
	int32_t intValue{};
	float_t floatValue{};
	std::string stringValue{};
	int32_t boolValue{};
	int64_t int64Value{};
	bool insertedFirstEntry = false;
	std::stringstream insertedRow;
	insertedRow << "INSERT INTO " << tableName << " values (";

	for (int32_t i = 0; i < numberOfColumns; i++) {
		if (i != 0) insertedRow << ", "; // Only append comma and space after first entry in row.
		switch (static_cast<eSqliteDataType>(ReadInt32(cdClientBuffer))) {
		case eSqliteDataType::NONE:
			BinaryIO::BinaryRead(cdClientBuffer, emptyValue);
			assert(emptyValue == 0);
			insertedRow << "NULL";
			break;

		case eSqliteDataType::INT32:
			intValue = ReadInt32(cdClientBuffer);
			insertedRow << intValue;
			break;

		case eSqliteDataType::REAL:
			BinaryIO::BinaryRead(cdClientBuffer, floatValue);
			insertedRow << std::fixed << std::setprecision(34) << floatValue; // maximum precision of floating point number
			break;

		case eSqliteDataType::TEXT_4:
		case eSqliteDataType::TEXT_8: {
			stringValue = ReadString(cdClientBuffer);
			size_t position = 0;

			// Need to escape quote with a double of ".
			while (position < stringValue.size()) {
				if (stringValue.at(position) == '\"') {
					stringValue.insert(position, "\"");
					position++;
				}
				position++;
			}
			insertedRow << "\"" << stringValue << "\"";
			break;
		}

		case eSqliteDataType::INT_BOOL:
			BinaryIO::BinaryRead(cdClientBuffer, boolValue);
			insertedRow << static_cast<bool>(boolValue);
			break;

		case eSqliteDataType::INT64:
			int64Value = ReadInt64(cdClientBuffer);
			insertedRow << std::to_string(int64Value);
			break;

		default:
			throw std::invalid_argument("Unsupported SQLite type encountered.");
			break;

		}
	}

	insertedRow << ");";

	auto copiedString = insertedRow.str();
	CDClientDatabase::ExecuteDML(copiedString);
	cdClientBuffer.seekg(prevPosition);
}
