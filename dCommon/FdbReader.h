#ifndef FDBREADER_H
#define FDBREADER_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FdbMappedFile.h"
#include "eSqliteDataType.h"

/**
 * Reads the client's cdclient.fdb in place, without loading it.
 *
 * Layout (all integers little-endian, every pointer an absolute u32 file offset, -1 meaning none):
 *   file header:   u32 table count, ptr to table header array
 *   table header:  ptr to column header, ptr to row top header (one pair per table, sorted by name)
 *   column header: u32 column count, ptr to table name, ptr to column array
 *   column:        u32 data type, ptr to column name
 *   row top:       u32 bucket count (a power of two, may be 0), ptr to bucket array
 *   bucket array:  one ptr per bucket to the first row info of the bucket
 *   row info:      ptr to row data, ptr to the next row info in the same bucket
 *   row data:      u32 field count, ptr to field array
 *   field:         u32 data type, u32 value
 *
 * A field value holds the number itself for int32, bool and float, and a pointer for int64
 * (to 8 bytes) and text (to a null-terminated latin-1 string). A row sits in bucket
 * (u32)(first field) % bucket count, for int32 first columns. Tables are found by name and
 * rows by their first column, so a lookup only touches a few pages of the file.
 */
class FdbReader {
public:
	class Row;
	class Table;

	struct Column {
		std::string name;
		eSqliteDataType type{};
	};

	class Row {
	public:
		[[nodiscard]] uint32_t GetFieldCount() const { return m_FieldCount; }

		// NONE for a null field or a column past the end of the row
		[[nodiscard]] eSqliteDataType GetType(uint32_t column) const;

		[[nodiscard]] bool IsNull(uint32_t column) const { return GetType(column) == eSqliteDataType::NONE; }

		// These convert between types the way SQLite does for the converted CDServer.sqlite,
		// and return nullValue for a null field.
		[[nodiscard]] int32_t GetInt(uint32_t column, int32_t nullValue = 0) const;
		[[nodiscard]] int64_t GetInt64(uint32_t column, int64_t nullValue = 0) const;
		[[nodiscard]] float GetFloat(uint32_t column, float nullValue = 0.0f) const;
		[[nodiscard]] bool GetBool(uint32_t column, bool nullValue = false) const { return GetInt(column, nullValue ? 1 : 0) != 0; }

		// UTF-8
		[[nodiscard]] std::string GetString(uint32_t column, std::string_view nullValue = "") const;

		// The latin-1 bytes as stored, for text fields; empty otherwise
		[[nodiscard]] std::string_view GetRawString(uint32_t column) const;

	private:
		friend class FdbReader;
		friend class Table;
		Row(const FdbReader* reader, uint32_t fieldsOffset, uint32_t fieldCount)
			: m_Reader(reader), m_FieldsOffset(fieldsOffset), m_FieldCount(fieldCount) {}

		bool ReadField(uint32_t column, eSqliteDataType& type, uint32_t& value) const;

		const FdbReader* m_Reader;
		uint32_t m_FieldsOffset;
		uint32_t m_FieldCount;
	};

	class Table {
	public:
		[[nodiscard]] const std::string& GetName() const { return m_Name; }
		[[nodiscard]] const std::vector<Column>& GetColumns() const { return m_Columns; }
		[[nodiscard]] uint32_t GetBucketCount() const { return m_BucketCount; }

		// The column's index, or -1 when the table has no such column
		[[nodiscard]] int32_t GetColumnIndex(std::string_view name) const;

		// Calls f(const Row&) for each row whose first column equals key, in file order
		template<typename F>
		void ForEachRowWithKey(int64_t key, F&& f) const {
			if (m_BucketCount == 0) return;
			const uint32_t bucket = static_cast<uint32_t>(static_cast<uint64_t>(key) % m_BucketCount);
			WalkBucket(bucket, [&](const Row& row) {
				if (KeyOf(row) == key) f(row);
			});
		}

		// The first row whose first column equals key
		[[nodiscard]] std::optional<Row> FindFirst(int64_t key) const;

		// Calls f(const Row&) for every row, bucket by bucket
		template<typename F>
		void ForEachRow(F&& f) const {
			for (uint32_t bucket = 0; bucket < m_BucketCount; bucket++) WalkBucket(bucket, f);
		}

		// The first column as an integer, the value rows are bucketed by
		[[nodiscard]] static std::optional<int64_t> KeyOf(const Row& row);

	private:
		friend class FdbReader;

		template<typename F>
		void WalkBucket(uint32_t bucket, F&& f) const {
			uint32_t rowInfo = 0;
			if (!m_Reader->ReadU32(m_BucketArrayOffset + static_cast<uint64_t>(bucket) * 4, rowInfo)) return;
			// A bound on the chain length so a damaged file can't loop forever
			uint64_t guard = m_Reader->GetSize() / 8;
			while (rowInfo != NO_POINTER && guard-- > 0) {
				uint32_t rowData = 0;
				uint32_t next = NO_POINTER;
				if (!m_Reader->ReadU32(rowInfo, rowData) || !m_Reader->ReadU32(static_cast<uint64_t>(rowInfo) + 4, next)) return;
				uint32_t fieldCount = 0;
				uint32_t fields = 0;
				if (m_Reader->ReadU32(rowData, fieldCount) && m_Reader->ReadU32(static_cast<uint64_t>(rowData) + 4, fields) &&
					m_Reader->InBounds(fields, static_cast<uint64_t>(fieldCount) * 8)) {
					f(Row(m_Reader, fields, fieldCount));
				}
				rowInfo = next;
			}
		}

		const FdbReader* m_Reader = nullptr;
		std::string m_Name;
		std::vector<Column> m_Columns;
		uint32_t m_BucketCount = 0;
		uint32_t m_BucketArrayOffset = 0;
	};

	static constexpr uint32_t NO_POINTER = 0xFFFFFFFF;

	FdbReader() = default;
	FdbReader(const FdbReader&) = delete;
	FdbReader& operator=(const FdbReader&) = delete;

	/**
	 * Opens an fdb file and reads its table and column headers (the rows stay in the file).
	 *
	 * @param path The fdb file
	 * @param allowMapping false reads the file into memory instead of mapping it
	 * @return true if the file was opened and its headers are sound
	 */
	bool Open(const std::filesystem::path& path, bool allowMapping = true);

	void Close();

	[[nodiscard]] bool IsOpen() const { return m_File.IsOpen(); }
	[[nodiscard]] bool IsMapped() const { return m_File.IsMapped(); }
	[[nodiscard]] uint64_t GetSize() const { return m_File.GetSize(); }

	// The table with this exact name, or nullptr
	[[nodiscard]] const Table* GetTable(std::string_view name) const;

	[[nodiscard]] const std::vector<Table>& GetTables() const { return m_Tables; }

	// Little-endian reads that fail instead of reading past the end of the file
	[[nodiscard]] bool ReadU32(uint64_t offset, uint32_t& out) const;
	[[nodiscard]] bool ReadI64(uint64_t offset, int64_t& out) const;
	[[nodiscard]] bool InBounds(uint64_t offset, uint64_t length) const;
	// The null-terminated string at offset, without the terminator
	[[nodiscard]] std::optional<std::string_view> ReadCString(uint64_t offset) const;

private:
	bool ReadHeaders();

	FdbMappedFile m_File;
	std::vector<Table> m_Tables;
	std::unordered_map<std::string_view, uint32_t> m_TableIndex;
};

#endif // FDBREADER_H
