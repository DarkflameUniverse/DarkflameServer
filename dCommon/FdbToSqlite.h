#ifndef __FDBTOSQLITE__H__
#define __FDBTOSQLITE__H__

#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>

#include "AssetManager.h"

enum class eSqliteDataType : int32_t;

namespace FdbToSqlite {
	class Convert {
	public:
		/**
		 * Create a new convert object with an input .fdb file and an output binary path.
		 * 
		 * @param inputFile The file which ends in .fdb to be converted
		 * @param binaryPath The base path where the file will be saved
		 */
		Convert(std::string binaryOutPath);

		/**
		 * Converts the input file to sqlite.  Calling multiple times is safe.
		 * 
		 * @return true if the database was converted properly, false otherwise. 
		 */
		bool ConvertDatabase(AssetStream& buffer);

		/**
		 * @brief Reads a 32 bit int from the fdb file.
		 * 
		 * @return The read value
		 */
		int32_t ReadInt32(std::istream& cdClientBuffer);

		/**
		 * @brief Reads a 64 bit integer from the fdb file.
		 * 
		 * @return The read value
		 */
		int64_t ReadInt64(std::istream& cdClientBuffer);

		/**
		 * @brief Reads a string from the fdb file.
		 * 
		 * @return The read string
		 * 
		 * TODO This needs to be translated to latin-1!
		 */
		std::string ReadString(std::istream& cdClientBuffer);

		/**
		 * @brief Seeks to a pointer position.
		 * 
		 * @return The previous position before the seek
		 */
		int32_t SeekPointer(std::istream& cdClientBuffer);

		/**
		 * @brief Reads a column header from the fdb file and creates the table in the database
		 * 
		 * @return The table name
		 */
		std::string ReadColumnHeader(std::istream& cdClientBuffer);

		/**
		 * @brief Read the tables from the fdb file.
		 * 
		 * @param numberOfTables The number of tables to read
		 */
		void ReadTables(int32_t& numberOfTables, std::istream& cdClientBuffer);

		/**
		 * @brief Reads the columns from the fdb file.
		 * 
		 * @param numberOfColumns The number of columns to read
		 * @return All columns of the table formatted for a sql query
		 */
		std::string ReadColumns(int32_t& numberOfColumns, std::istream& cdClientBuffer);

		/**
		 * @brief Reads the row header from the fdb file.
		 * 
		 * @param tableName The tables name
		 */
		void ReadRowHeader(std::string& tableName, std::istream& cdClientBuffer);

		/**
		 * @brief Read the rows from the fdb file.,
		 * 
		 * @param numberOfAllocatedRows The number of rows that were allocated.  Always a power of 2! 
		 * @param tableName The tables name.
		 */
		void ReadRows(int32_t& numberOfAllocatedRows, std::string& tableName, std::istream& cdClientBuffer);

		/**
		 * @brief Reads a row from the fdb file.
		 * 
		 * @param position The position to seek in the fdb to
		 * @param tableName The tables name
		 */
		void ReadRow(int32_t& position, std::string& tableName, std::istream& cdClientBuffer);

		/**
		 * @brief Reads the row info from the fdb file.
		 * 
		 * @param tableName The tables name
		 */
		void ReadRowInfo(std::string& tableName, std::istream& cdClientBuffer);

		/**
		 * @brief Reads each row and its values from the fdb file and inserts them into the database
		 * 
		 * @param numberOfColumns The number of columns to read in
		 * @param tableName The tables name
		 */
		void ReadRowValues(int32_t& numberOfColumns, std::string& tableName, std::istream& cdClientBuffer);
	private:

		/**
		 * Maps each sqlite data type to its string equivalent.
		 */
		static std::map<eSqliteDataType, std::string> m_SqliteType;

		/**
		 * Base path of the folder containing the fdb file
		 */
		std::string m_BasePath{};
		
		/**
		 * Whether or not a conversion was started.  If one was started, do not attempt to convert the file again.
		 */
		bool m_ConversionStarted{};

		/**
		 * The path where the CDServer will be stored
		 */
		std::string m_BinaryOutPath{};
	}; //! class FdbToSqlite
}; //! namespace FdbToSqlite

#endif  //!__FDBTOSQLITE__H__
