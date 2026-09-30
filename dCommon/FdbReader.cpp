#include "FdbReader.h"

#include <charconv>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "GeneralUtils.h"

namespace {
	// Little-endian decode from bytes, independent of host byte order and alignment
	uint32_t DecodeU32(const uint8_t* bytes) {
		return static_cast<uint32_t>(bytes[0]) |
			(static_cast<uint32_t>(bytes[1]) << 8) |
			(static_cast<uint32_t>(bytes[2]) << 16) |
			(static_cast<uint32_t>(bytes[3]) << 24);
	}

	uint64_t DecodeU64(const uint8_t* bytes) {
		return static_cast<uint64_t>(DecodeU32(bytes)) | (static_cast<uint64_t>(DecodeU32(bytes + 4)) << 32);
	}

	float BitsToFloat(uint32_t bits) {
		float value;
		static_assert(sizeof(value) == sizeof(bits));
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	// sqlite3_column_int on text: the leading integer, 0 if there is none
	int64_t LeadingInteger(std::string_view text) {
		size_t start = 0;
		while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) start++;
		if (start < text.size() && text[start] == '+') start++;
		int64_t value = 0;
		std::from_chars(text.data() + start, text.data() + text.size(), value);
		return value;
	}

	bool IsText(eSqliteDataType type) {
		return type == eSqliteDataType::TEXT_4 || type == eSqliteDataType::TEXT_8;
	}
}

bool FdbReader::Open(const std::filesystem::path& path, bool allowMapping) {
	Close();
	if (!m_File.Open(path, allowMapping)) return false;
	if (!ReadHeaders()) {
		Close();
		return false;
	}
	return true;
}

void FdbReader::Close() {
	m_TableIndex.clear();
	m_Tables.clear();
	m_File.Close();
}

bool FdbReader::InBounds(uint64_t offset, uint64_t length) const {
	const uint64_t size = m_File.GetSize();
	return offset <= size && length <= size - offset;
}

bool FdbReader::ReadU32(uint64_t offset, uint32_t& out) const {
	if (!InBounds(offset, 4)) return false;
	out = DecodeU32(m_File.GetData() + offset);
	return true;
}

bool FdbReader::ReadI64(uint64_t offset, int64_t& out) const {
	if (!InBounds(offset, 8)) return false;
	out = static_cast<int64_t>(DecodeU64(m_File.GetData() + offset));
	return true;
}

std::optional<std::string_view> FdbReader::ReadCString(uint64_t offset) const {
	if (!InBounds(offset, 0) || offset == m_File.GetSize()) return std::nullopt;
	const auto* start = m_File.GetData() + offset;
	const auto* end = static_cast<const uint8_t*>(std::memchr(start, 0, static_cast<size_t>(m_File.GetSize() - offset)));
	if (!end) return std::nullopt;
	return std::string_view(reinterpret_cast<const char*>(start), static_cast<size_t>(end - start));
}

bool FdbReader::ReadHeaders() {
	uint32_t tableCount = 0;
	uint32_t tableHeaders = 0;
	if (!ReadU32(0, tableCount) || !ReadU32(4, tableHeaders)) return false;
	if (!InBounds(tableHeaders, static_cast<uint64_t>(tableCount) * 8)) return false;

	m_Tables.reserve(tableCount);
	for (uint32_t i = 0; i < tableCount; i++) {
		const uint64_t entry = static_cast<uint64_t>(tableHeaders) + static_cast<uint64_t>(i) * 8;
		uint32_t columnHeader = 0;
		uint32_t rowTop = 0;
		if (!ReadU32(entry, columnHeader) || !ReadU32(entry + 4, rowTop)) return false;

		Table table;
		table.m_Reader = this;

		uint32_t columnCount = 0;
		uint32_t namePointer = 0;
		uint32_t columns = 0;
		if (!ReadU32(columnHeader, columnCount) || !ReadU32(static_cast<uint64_t>(columnHeader) + 4, namePointer) ||
			!ReadU32(static_cast<uint64_t>(columnHeader) + 8, columns)) return false;
		const auto name = ReadCString(namePointer);
		if (!name || !InBounds(columns, static_cast<uint64_t>(columnCount) * 8)) return false;
		table.m_Name = std::string(*name);

		table.m_Columns.reserve(columnCount);
		for (uint32_t c = 0; c < columnCount; c++) {
			const uint64_t column = static_cast<uint64_t>(columns) + static_cast<uint64_t>(c) * 8;
			uint32_t type = 0;
			uint32_t columnName = 0;
			if (!ReadU32(column, type) || !ReadU32(column + 4, columnName)) return false;
			const auto columnNameString = ReadCString(columnName);
			if (!columnNameString) return false;
			table.m_Columns.push_back({ GeneralUtils::Latin1ToUTF8(std::u8string_view(reinterpret_cast<const char8_t*>(columnNameString->data()), columnNameString->size())), static_cast<eSqliteDataType>(type) });
		}

		if (!ReadU32(rowTop, table.m_BucketCount) || !ReadU32(static_cast<uint64_t>(rowTop) + 4, table.m_BucketArrayOffset)) return false;
		if (!InBounds(table.m_BucketArrayOffset, static_cast<uint64_t>(table.m_BucketCount) * 4)) return false;

		m_Tables.push_back(std::move(table));
	}

	// Built after the vector stops growing, since the keys point into the table names
	for (uint32_t i = 0; i < m_Tables.size(); i++) m_TableIndex.emplace(m_Tables[i].m_Name, i);
	return true;
}

const FdbReader::Table* FdbReader::GetTable(std::string_view name) const {
	const auto it = m_TableIndex.find(name);
	return it == m_TableIndex.end() ? nullptr : &m_Tables[it->second];
}

int32_t FdbReader::Table::GetColumnIndex(std::string_view name) const {
	for (uint32_t i = 0; i < m_Columns.size(); i++) {
		if (m_Columns[i].name == name) return static_cast<int32_t>(i);
	}
	return -1;
}

std::optional<FdbReader::Row> FdbReader::Table::FindFirst(int64_t key) const {
	std::optional<Row> found;
	ForEachRowWithKey(key, [&found](const Row& row) {
		if (!found) found = row;
	});
	return found;
}

std::optional<int64_t> FdbReader::Table::KeyOf(const Row& row) {
	switch (row.GetType(0)) {
	case eSqliteDataType::INT32:
	case eSqliteDataType::INT_BOOL:
		return row.GetInt(0);
	case eSqliteDataType::INT64:
		return row.GetInt64(0);
	default:
		return std::nullopt;
	}
}

bool FdbReader::Row::ReadField(uint32_t column, eSqliteDataType& type, uint32_t& value) const {
	if (column >= m_FieldCount) return false;
	const uint64_t field = static_cast<uint64_t>(m_FieldsOffset) + static_cast<uint64_t>(column) * 8;
	uint32_t rawType = 0;
	if (!m_Reader->ReadU32(field, rawType) || !m_Reader->ReadU32(field + 4, value)) return false;
	type = static_cast<eSqliteDataType>(rawType);
	return true;
}

eSqliteDataType FdbReader::Row::GetType(uint32_t column) const {
	eSqliteDataType type{};
	uint32_t value = 0;
	return ReadField(column, type, value) ? type : eSqliteDataType::NONE;
}

int64_t FdbReader::Row::GetInt64(uint32_t column, int64_t nullValue) const {
	eSqliteDataType type{};
	uint32_t value = 0;
	if (!ReadField(column, type, value)) return nullValue;
	switch (type) {
	case eSqliteDataType::INT32:
		return static_cast<int32_t>(value);
	case eSqliteDataType::INT_BOOL:
		// The conversion stores bools as 0 or 1
		return value != 0 ? 1 : 0;
	case eSqliteDataType::INT64: {
		int64_t wide = 0;
		return m_Reader->ReadI64(value, wide) ? wide : nullValue;
	}
	case eSqliteDataType::REAL:
		return static_cast<int64_t>(BitsToFloat(value));
	case eSqliteDataType::TEXT_4:
	case eSqliteDataType::TEXT_8:
		return LeadingInteger(GetRawString(column));
	default:
		return nullValue;
	}
}

int32_t FdbReader::Row::GetInt(uint32_t column, int32_t nullValue) const {
	// sqlite3_column_int keeps the low 32 bits of a wider integer
	if (IsNull(column)) return nullValue;
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(GetInt64(column, nullValue))));
}

float FdbReader::Row::GetFloat(uint32_t column, float nullValue) const {
	eSqliteDataType type{};
	uint32_t value = 0;
	if (!ReadField(column, type, value)) return nullValue;
	switch (type) {
	case eSqliteDataType::REAL:
		return BitsToFloat(value);
	case eSqliteDataType::INT32:
	case eSqliteDataType::INT_BOOL:
	case eSqliteDataType::INT64:
		return static_cast<float>(GetInt64(column, 0));
	case eSqliteDataType::TEXT_4:
	case eSqliteDataType::TEXT_8: {
		const std::string text(GetRawString(column));
		return static_cast<float>(std::strtod(text.c_str(), nullptr));
	}
	default:
		return nullValue;
	}
}

std::string_view FdbReader::Row::GetRawString(uint32_t column) const {
	eSqliteDataType type{};
	uint32_t value = 0;
	if (!ReadField(column, type, value) || !IsText(type)) return {};
	return m_Reader->ReadCString(value).value_or(std::string_view{});
}

std::string FdbReader::Row::GetString(uint32_t column, std::string_view nullValue) const {
	eSqliteDataType type{};
	uint32_t value = 0;
	if (!ReadField(column, type, value)) return std::string(nullValue);
	switch (type) {
	case eSqliteDataType::TEXT_4:
	case eSqliteDataType::TEXT_8: {
		const auto raw = GetRawString(column);
		return GeneralUtils::Latin1ToUTF8(std::u8string_view(reinterpret_cast<const char8_t*>(raw.data()), raw.size()));
	}
	case eSqliteDataType::INT32:
	case eSqliteDataType::INT_BOOL:
	case eSqliteDataType::INT64:
		return std::to_string(GetInt64(column, 0));
	case eSqliteDataType::REAL: {
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "%.15g", static_cast<double>(BitsToFloat(value)));
		return buffer;
	}
	default:
		return std::string(nullValue);
	}
}
