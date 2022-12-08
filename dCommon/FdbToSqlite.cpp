#include "FdbToSqlite.h"

#include <map>
#include <fstream>
#include <cassert>
#include <iomanip>

#include "BinaryIO.h"
#include "CDClientDatabase.h"
#include "GeneralUtils.h"
#include "Game.h"
#include "dLogger.h"

#include "eSqliteDataType.h"

std::map<eSqliteDataType, std::string> FdbToSqlite::Convert::sqliteType = {
			{ eSqliteDataType::NONE, "none"},
			{ eSqliteDataType::INT32, "int32"},
			{ eSqliteDataType::REAL, "real"},
			{ eSqliteDataType::TEXT_4, "text_4"},
			{ eSqliteDataType::INT_BOOL, "int_bool"},
			{ eSqliteDataType::INT64, "int64"},
			{ eSqliteDataType::TEXT_8, "text_8"}
};

FdbToSqlite::Convert::Convert(std::string basePath) {
	this->basePath = basePath;
}

bool FdbToSqlite::Convert::ConvertDatabase() {
	fdb.open(basePath + "/cdclient.fdb", std::ios::binary);

	try {
		CDClientDatabase::Connect(basePath + "/CDServer.sqlite");

		CDClientDatabase::ExecuteQuery("BEGIN TRANSACTION;");

		int32_t numberOfTables = ReadInt32();
		ReadTables(numberOfTables);

		CDClientDatabase::ExecuteQuery("COMMIT;");
	} catch (CppSQLite3Exception& e) {
		Game::logger->Log("FdbToSqlite", "Encountered error %s converting FDB to SQLite", e.errorMessage());
		return false;
	}

	fdb.close();
	return true;
}

int32_t FdbToSqlite::Convert::ReadInt32() {
	int32_t nextInt{};
	BinaryIO::BinaryRead(fdb, nextInt);
	return nextInt;
}

int64_t FdbToSqlite::Convert::ReadInt64() {
	int32_t prevPosition = SeekPointer();

	int64_t value{};
	BinaryIO::BinaryRead(fdb, value);

	fdb.seekg(prevPosition);
	return value;
}

std::string FdbToSqlite::Convert::ReadString() {
	int32_t prevPosition = SeekPointer();

	auto readString = BinaryIO::ReadString(fdb);

	fdb.seekg(prevPosition);
	return readString;
}

int32_t FdbToSqlite::Convert::SeekPointer() {
	int32_t position{};
	BinaryIO::BinaryRead(fdb, position);
	int32_t prevPosition = fdb.tellg();
	fdb.seekg(position);
	return prevPosition;
}

std::string FdbToSqlite::Convert::ReadColumnHeader() {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfColumns = ReadInt32();
	std::string tableName = ReadString();

	auto columns = ReadColumns(numberOfColumns);
	std::string newTable = "CREATE TABLE IF NOT EXISTS '" + tableName + "' (" + columns + ");";
	CDClientDatabase::ExecuteDML(newTable);

	fdb.seekg(prevPosition);

	return tableName;
}

void FdbToSqlite::Convert::ReadTables(int32_t& numberOfTables) {
	int32_t prevPosition = SeekPointer();

	for (int32_t i = 0; i < numberOfTables; i++) {
		auto columnHeader = ReadColumnHeader();
		ReadRowHeader(columnHeader);
	}

	fdb.seekg(prevPosition);
}

std::string FdbToSqlite::Convert::ReadColumns(int32_t& numberOfColumns) {
	std::stringstream columnsToCreate;
	int32_t prevPosition = SeekPointer();

	std::string name{};
	eSqliteDataType dataType{};
	for (int32_t i = 0; i < numberOfColumns; i++) {
		if (i != 0) columnsToCreate << ", ";
		dataType = static_cast<eSqliteDataType>(ReadInt32());
		name = ReadString();
		columnsToCreate << "'" << name << "' " << FdbToSqlite::Convert::sqliteType[dataType];
	}

	fdb.seekg(prevPosition);
	return columnsToCreate.str();
}

void FdbToSqlite::Convert::ReadRowHeader(std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfAllocatedRows = ReadInt32();
	if (numberOfAllocatedRows != 0) assert((numberOfAllocatedRows & (numberOfAllocatedRows - 1)) == 0);  // assert power of 2 allocation size
	ReadRows(numberOfAllocatedRows, tableName);

	fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRows(int32_t& numberOfAllocatedRows, std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t rowid = 0;
	for (int32_t row = 0; row < numberOfAllocatedRows; row++) {
		int32_t rowPointer = ReadInt32();
		if (rowPointer == -1) rowid++;
		else ReadRow(rowid, rowPointer, tableName);
	}

	fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRow(int32_t& rowid, int32_t& position, std::string& tableName) {
	int32_t prevPosition = fdb.tellg();
	fdb.seekg(position);

	while (true) {
		ReadRowInfo(tableName);
		int32_t linked = ReadInt32();

		rowid += 1;

		if (linked == -1) break;

		fdb.seekg(linked);
	}

	fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRowInfo(std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfColumns = ReadInt32();
	ReadRowValues(numberOfColumns, tableName);

	fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRowValues(int32_t& numberOfColumns, std::string& tableName) {
	int32_t prevPosition = SeekPointer();

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
		switch (static_cast<eSqliteDataType>(ReadInt32())) {
		case eSqliteDataType::NONE:
			BinaryIO::BinaryRead(fdb, emptyValue);
			assert(emptyValue == 0);
			insertedRow << "NULL";
			break;

		case eSqliteDataType::INT32:
			intValue = ReadInt32();
			insertedRow << intValue;
			break;

		case eSqliteDataType::REAL:
			BinaryIO::BinaryRead(fdb, floatValue);
			insertedRow << std::fixed << std::setprecision(34) << floatValue; // maximum precision of floating point number
			break;

		case eSqliteDataType::TEXT_4:
		case eSqliteDataType::TEXT_8: {
			stringValue = ReadString();
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
			BinaryIO::BinaryRead(fdb, boolValue);
			insertedRow << static_cast<bool>(boolValue);
			break;

		case eSqliteDataType::INT64:
			int64Value = ReadInt64();
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
	fdb.seekg(prevPosition);
}
