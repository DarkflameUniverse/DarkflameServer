#ifndef __FDBTOSQLITE__H__
#define __FDBTOSQLITE__H__

#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>

enum class eSqliteDataType : int32_t;

namespace FdbToSqlite {
	class Convert {
	public:
		Convert(std::string inputFile);

		bool ConvertDatabase();

		int32_t ReadInt32();

		int64_t ReadInt64();

		std::string ReadString();

		int32_t SeekPointer();

		std::string ReadColumnHeader();

		void ReadTables(int32_t& numberOfTables);

		std::string ReadColumns(int32_t& numberOfColumns);

		void ReadRowHeader(std::string& tableName);

		void ReadRows(int32_t& numberOfAllocatedRows, std::string& tableName);

		void ReadRow(int32_t& rowid, int32_t& position, std::string& tableName);

		void ReadRowInfo(std::string& tableName);

		void ReadRowValues(int32_t& numberOfColumns, std::string& tableName);
	private:
		static std::map<eSqliteDataType, std::string> sqliteType;
		std::string basePath{};
		std::ifstream fdb{};
	}; // class FdbToSqlite
}; //! namespace FdbToSqlite

#endif  //!__FDBTOSQLITE__H__
