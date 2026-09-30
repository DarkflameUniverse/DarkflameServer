#include "CDFdb.h"

#include <cstring>
#include <memory>

#include "CDClientDatabase.h"

namespace {
	std::unique_ptr<FdbReader> g_Fdb;
	std::unique_ptr<FdbReader> g_Retired;

	// Column name -> index for each table handed out by GetTable, so RowFields looks up names in O(1)
	std::unordered_map<const FdbReader::Table*, std::unordered_map<std::string, int32_t>> g_ColumnIndices;

	constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;

	// A value hash that is the same for a value read from either file
	class ValueHasher {
	public:
		void Null() { Byte(0); }

		void Integer(int64_t value) {
			Byte(1);
			const auto bits = static_cast<uint64_t>(value);
			for (uint32_t i = 0; i < 8; i++) Byte(static_cast<uint8_t>(bits >> (i * 8)));
		}

		void Real(float value) {
			Byte(2);
			uint32_t bits = 0;
			std::memcpy(&bits, &value, sizeof(bits));
			for (uint32_t i = 0; i < 4; i++) Byte(static_cast<uint8_t>(bits >> (i * 8)));
		}

		void Text(std::string_view value) {
			Byte(3);
			for (const char c : value) Byte(static_cast<uint8_t>(c));
			// Length ends the value so ("ab","c") and ("a","bc") differ
			Integer(static_cast<int64_t>(value.size()));
		}

		// splitmix64 finish so row hashes summed per key don't cancel out by accident
		[[nodiscard]] uint64_t Finish() const {
			uint64_t z = m_Hash + 0x9E3779B97F4A7C15ULL;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
			return z ^ (z >> 31);
		}

	private:
		void Byte(uint8_t byte) {
			m_Hash ^= byte;
			m_Hash *= FNV_PRIME;
		}

		uint64_t m_Hash = FNV_OFFSET;
	};

	uint64_t HashFdbRow(const FdbReader::Row& row, uint32_t columnCount) {
		ValueHasher hasher;
		for (uint32_t c = 0; c < columnCount; c++) {
			switch (row.GetType(c)) {
			case eSqliteDataType::INT32:
			case eSqliteDataType::INT_BOOL:
			case eSqliteDataType::INT64:
				hasher.Integer(row.GetInt64(c));
				break;
			case eSqliteDataType::REAL:
				hasher.Real(row.GetFloat(c));
				break;
			case eSqliteDataType::TEXT_4:
			case eSqliteDataType::TEXT_8:
				hasher.Text(row.GetString(c));
				break;
			default:
				hasher.Null();
				break;
			}
		}
		return hasher.Finish();
	}

	uint64_t HashSqliteRow(CppSQLite3Query& query, int32_t columnCount) {
		ValueHasher hasher;
		for (int32_t c = 0; c < columnCount; c++) {
			switch (query.fieldDataType(c)) {
			case SQLITE_INTEGER:
				hasher.Integer(query.getInt64Field(c));
				break;
			case SQLITE_FLOAT:
				// The fdb holds 32-bit floats; the conversion printed them exactly, so this rounds back to the same float
				hasher.Real(static_cast<float>(query.getFloatField(c)));
				break;
			case SQLITE_TEXT:
				hasher.Text(query.getStringField(c));
				break;
			case SQLITE_NULL:
				hasher.Null();
				break;
			default: {
				// Blobs never come out of the conversion; hash them apart from text
				int length = 0;
				const auto* blob = query.getBlobField(c, length);
				hasher.Integer(-1);
				hasher.Text(std::string_view(reinterpret_cast<const char*>(blob), blob ? static_cast<uint32_t>(length) : 0));
				break;
			}
			}
		}
		return hasher.Finish();
	}
}

bool CDFdb::Open(const std::filesystem::path& path, bool allowMapping) {
	Close();
	std::error_code error;
	if (path.empty() || !std::filesystem::is_regular_file(path, error)) return false;

	auto reader = std::make_unique<FdbReader>();
	if (!reader->Open(path, allowMapping)) return false;
	g_Fdb = std::move(reader);
	return true;
}

void CDFdb::Close() {
	g_ColumnIndices.clear();
	g_Fdb.reset();
}

void CDFdb::Retire() {
	g_ColumnIndices.clear();
	if (g_Fdb) g_Retired = std::move(g_Fdb);
}

const FdbReader* CDFdb::Get() {
	return g_Fdb.get();
}

const FdbReader::Table* CDFdb::GetTable(const std::string& name) {
	if (!g_Fdb || !CDClientDatabase::isConnected) return nullptr;
	const auto* table = g_Fdb->GetTable(name);
	if (!table || table->GetColumns().empty()) return nullptr;

	const auto firstType = table->GetColumns()[0].type;
	if (firstType != eSqliteDataType::INT32 && firstType != eSqliteDataType::INT64) return nullptr;

	// Same columns in the same order as the SQLite table
	try {
		auto query = CDClientDatabase::ExecuteQuery("SELECT * FROM \"" + name + "\" LIMIT 0;");
		const auto& columns = table->GetColumns();
		if (query.numFields() != static_cast<int>(columns.size())) return nullptr;
		for (int32_t i = 0; i < query.numFields(); i++) {
			if (columns[i].name != query.fieldName(i)) return nullptr;
		}
	} catch (const CppSQLite3Exception&) {
		return nullptr;
	}

	auto& indices = g_ColumnIndices[table];
	if (indices.empty()) {
		for (uint32_t i = 0; i < table->GetColumns().size(); i++) indices.emplace(table->GetColumns()[i].name, static_cast<int32_t>(i));
	}
	return table;
}

std::optional<std::vector<int64_t>> CDFdb::FindChangedKeys(const FdbReader::Table& table) {
	if (!CDClientDatabase::isConnected) return std::nullopt;
	const auto columnCount = static_cast<uint32_t>(table.GetColumns().size());

	// Per key: the sum of the fdb row hashes minus the sum of the SQLite row hashes
	std::unordered_map<int64_t, uint64_t> balance;
	balance.reserve(table.GetBucketCount());

	table.ForEachRow([&](const FdbReader::Row& row) {
		const auto key = FdbReader::Table::KeyOf(row);
		// A row without an integer key can't be looked up by key; count it so the table isn't used
		balance[key.value_or(INT64_MIN)] += HashFdbRow(row, columnCount);
	});

	try {
		auto query = CDClientDatabase::ExecuteQuery("SELECT * FROM \"" + table.GetName() + "\";");
		if (query.numFields() != static_cast<int>(columnCount)) return std::nullopt;
		while (!query.eof()) {
			const int64_t key = query.fieldDataType(0) == SQLITE_INTEGER ? query.getInt64Field(0) : INT64_MIN;
			balance[key] -= HashSqliteRow(query, static_cast<int32_t>(columnCount));
			query.nextRow();
		}
		query.finalize();
	} catch (const CppSQLite3Exception&) {
		return std::nullopt;
	}

	std::vector<int64_t> changed;
	for (const auto& [key, sum] : balance) {
		if (sum == 0) continue;
		if (key == INT64_MIN) return std::nullopt;
		changed.push_back(key);
	}
	return changed;
}

CDFdb::RowFields::RowFields(const FdbReader::Table& table, const FdbReader::Row& row) : m_Table(table), m_Row(row) {}

int32_t CDFdb::RowFields::Column(const char* field) const {
	const auto table = g_ColumnIndices.find(&m_Table);
	if (table == g_ColumnIndices.end()) return m_Table.GetColumnIndex(field);
	const auto it = table->second.find(field);
	return it == table->second.end() ? -1 : it->second;
}

int CDFdb::RowFields::getIntField(const char* field, int nullValue) const {
	const auto column = Column(field);
	return column < 0 ? nullValue : m_Row.GetInt(static_cast<uint32_t>(column), nullValue);
}

int64_t CDFdb::RowFields::getInt64Field(const char* field, int64_t nullValue) const {
	const auto column = Column(field);
	return column < 0 ? nullValue : m_Row.GetInt64(static_cast<uint32_t>(column), nullValue);
}

double CDFdb::RowFields::getFloatField(const char* field, double nullValue) const {
	const auto column = Column(field);
	if (column < 0 || m_Row.IsNull(static_cast<uint32_t>(column))) return nullValue;
	return m_Row.GetFloat(static_cast<uint32_t>(column));
}

std::string CDFdb::RowFields::getStringField(const char* field, const char* nullValue) const {
	const auto column = Column(field);
	return column < 0 ? std::string(nullValue) : m_Row.GetString(static_cast<uint32_t>(column), nullValue);
}

bool CDFdb::RowFields::fieldIsNull(const char* field) const {
	const auto column = Column(field);
	return column < 0 || m_Row.IsNull(static_cast<uint32_t>(column));
}
