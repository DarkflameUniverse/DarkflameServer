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

std::map<eSqliteDataType, std::string> FdbToSqlite::Convert::m_SqliteType = {
			{ eSqliteDataType::NONE, "none"},
			{ eSqliteDataType::INT32, "int32"},
			{ eSqliteDataType::REAL, "real"},
			{ eSqliteDataType::TEXT_4, "text_4"},
			{ eSqliteDataType::INT_BOOL, "int_bool"},
			{ eSqliteDataType::INT64, "int64"},
			{ eSqliteDataType::TEXT_8, "text_8"}
};

FdbToSqlite::Convert::Convert(std::string basePath, std::string binaryOutPath) {
	this->m_BasePath = basePath;
	this->m_BinaryOutPath = binaryOutPath;
	m_Fdb.open(m_BasePath + "/cdclient.fdb", std::ios::binary);
}

FdbToSqlite::Convert::~Convert() {
	this->m_Fdb.close();
}

bool FdbToSqlite::Convert::ConvertDatabase() {
	if (m_ConversionStarted) return false;
	this->m_ConversionStarted = true;
	try {
		CDClientDatabase::Connect(m_BinaryOutPath + "/CDServer.sqlite");

		CDClientDatabase::ExecuteQuery("BEGIN TRANSACTION;");

		int32_t numberOfTables = ReadInt32();
		ReadTables(numberOfTables);

		CDClientDatabase::ExecuteQuery("COMMIT;");
	} catch (CppSQLite3Exception& e) {
		Game::logger->Log("FdbToSqlite", "Encountered error %s converting FDB to SQLite", e.errorMessage());
		return false;
	}

	return true;
}

int32_t FdbToSqlite::Convert::ReadInt32() {
	int32_t nextInt{};
	BinaryIO::BinaryRead(m_Fdb, nextInt);
	return nextInt;
}

int64_t FdbToSqlite::Convert::ReadInt64() {
	int32_t prevPosition = SeekPointer();

	int64_t value{};
	BinaryIO::BinaryRead(m_Fdb, value);

	m_Fdb.seekg(prevPosition);
	return value;
}

std::string FdbToSqlite::Convert::ReadString() {
	int32_t prevPosition = SeekPointer();

	auto readString = BinaryIO::ReadString(m_Fdb);

	m_Fdb.seekg(prevPosition);
	return readString;
}

int32_t FdbToSqlite::Convert::SeekPointer() {
	int32_t position{};
	BinaryIO::BinaryRead(m_Fdb, position);
	int32_t prevPosition = m_Fdb.tellg();
	m_Fdb.seekg(position);
	return prevPosition;
}

std::string FdbToSqlite::Convert::ReadColumnHeader() {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfColumns = ReadInt32();
	std::string tableName = ReadString();

	auto columns = ReadColumns(numberOfColumns);
	std::string newTable = "CREATE TABLE IF NOT EXISTS '" + tableName + "' (" + columns + ");";
	CDClientDatabase::ExecuteDML(newTable);

	m_Fdb.seekg(prevPosition);

	return tableName;
}

void FdbToSqlite::Convert::ReadTables(int32_t& numberOfTables) {
	int32_t prevPosition = SeekPointer();

	for (int32_t i = 0; i < numberOfTables; i++) {
		auto columnHeader = ReadColumnHeader();
		ReadRowHeader(columnHeader);
	}

	m_Fdb.seekg(prevPosition);
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
		columnsToCreate << "'" << name << "' " << FdbToSqlite::Convert::m_SqliteType[dataType];
	}

	m_Fdb.seekg(prevPosition);
	return columnsToCreate.str();
}

void FdbToSqlite::Convert::ReadRowHeader(std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfAllocatedRows = ReadInt32();
	if (numberOfAllocatedRows != 0) assert((numberOfAllocatedRows & (numberOfAllocatedRows - 1)) == 0);  // assert power of 2 allocation size
	ReadRows(numberOfAllocatedRows, tableName);

	m_Fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRows(int32_t& numberOfAllocatedRows, std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t rowid = 0;
	for (int32_t row = 0; row < numberOfAllocatedRows; row++) {
		int32_t rowPointer = ReadInt32();
		if (rowPointer == -1) rowid++;
		else ReadRow(rowPointer, tableName);
	}

	m_Fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRow(int32_t& position, std::string& tableName) {
	int32_t prevPosition = m_Fdb.tellg();
	m_Fdb.seekg(position);

	while (true) {
		ReadRowInfo(tableName);
		int32_t linked = ReadInt32();
		if (linked == -1) break;
		m_Fdb.seekg(linked);
	}

	m_Fdb.seekg(prevPosition);
}

void FdbToSqlite::Convert::ReadRowInfo(std::string& tableName) {
	int32_t prevPosition = SeekPointer();

	int32_t numberOfColumns = ReadInt32();
	ReadRowValues(numberOfColumns, tableName);

	m_Fdb.seekg(prevPosition);
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
			BinaryIO::BinaryRead(m_Fdb, emptyValue);
			assert(emptyValue == 0);
			insertedRow << "NULL";
			break;

		case eSqliteDataType::INT32:
			intValue = ReadInt32();
			insertedRow << intValue;
			break;

		case eSqliteDataType::REAL:
			BinaryIO::BinaryRead(m_Fdb, floatValue);
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
			BinaryIO::BinaryRead(m_Fdb, boolValue);
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
	m_Fdb.seekg(prevPosition);
}
