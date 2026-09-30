#ifndef FDBTESTWRITER_H
#define FDBTESTWRITER_H

// Writes small fdb files for tests, in the client's layout (see FdbReader.h)

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "eSqliteDataType.h"

namespace FdbTestWriter {
	struct Value {
		eSqliteDataType type = eSqliteDataType::NONE;
		int64_t integer = 0;
		float real = 0.0f;
		std::string text; // latin-1 bytes

		static Value Null() { return {}; }
		static Value Int(int32_t v) { Value r; r.type = eSqliteDataType::INT32; r.integer = v; return r; }
		static Value Bool(bool v) { Value r; r.type = eSqliteDataType::INT_BOOL; r.integer = v ? 1 : 0; return r; }
		static Value Int64(int64_t v) { Value r; r.type = eSqliteDataType::INT64; r.integer = v; return r; }
		static Value Real(float v) { Value r; r.type = eSqliteDataType::REAL; r.real = v; return r; }
		static Value Text(std::string v, bool wide = false) {
			Value r; r.type = wide ? eSqliteDataType::TEXT_8 : eSqliteDataType::TEXT_4; r.text = std::move(v); return r;
		}
	};

	struct Column {
		std::string name;
		eSqliteDataType type;
	};

	struct Table {
		std::string name;
		std::vector<Column> columns;
		uint32_t bucketCount = 0;
		std::vector<std::vector<Value>> rows;
	};

	class Buffer {
	public:
		uint32_t Alloc(uint32_t size) {
			const auto offset = static_cast<uint32_t>(m_Bytes.size());
			m_Bytes.resize(m_Bytes.size() + size);
			return offset;
		}

		void PutU32(uint32_t offset, uint32_t value) {
			for (uint32_t i = 0; i < 4; i++) m_Bytes[offset + i] = static_cast<uint8_t>(value >> (i * 8));
		}

		uint32_t String(const std::string& text) {
			const auto offset = Alloc(static_cast<uint32_t>(text.size()) + 1);
			if (!text.empty()) std::memcpy(m_Bytes.data() + offset, text.data(), text.size());
			return offset;
		}

		uint32_t I64(int64_t value) {
			const auto offset = Alloc(8);
			PutU32(offset, static_cast<uint32_t>(static_cast<uint64_t>(value)));
			PutU32(offset + 4, static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32));
			return offset;
		}

		std::vector<uint8_t>& Bytes() { return m_Bytes; }

	private:
		std::vector<uint8_t> m_Bytes;
	};

	inline std::vector<uint8_t> Write(const std::vector<Table>& tables) {
		constexpr uint32_t NONE = 0xFFFFFFFF;
		Buffer out;
		out.Alloc(8);
		const auto headers = out.Alloc(static_cast<uint32_t>(tables.size()) * 8);
		out.PutU32(0, static_cast<uint32_t>(tables.size()));
		out.PutU32(4, headers);

		for (uint32_t t = 0; t < tables.size(); t++) {
			const auto& table = tables[t];

			const auto columnHeader = out.Alloc(12);
			const auto columns = out.Alloc(static_cast<uint32_t>(table.columns.size()) * 8);
			out.PutU32(columnHeader, static_cast<uint32_t>(table.columns.size()));
			out.PutU32(columnHeader + 4, out.String(table.name));
			out.PutU32(columnHeader + 8, columns);
			for (uint32_t c = 0; c < table.columns.size(); c++) {
				out.PutU32(columns + c * 8, static_cast<uint32_t>(table.columns[c].type));
				out.PutU32(columns + c * 8 + 4, out.String(table.columns[c].name));
			}

			const auto rowTop = out.Alloc(8);
			const auto buckets = out.Alloc(table.bucketCount * 4);
			out.PutU32(rowTop, table.bucketCount);
			out.PutU32(rowTop + 4, buckets);
			std::vector<uint32_t> lastInBucket(table.bucketCount, NONE);
			for (uint32_t b = 0; b < table.bucketCount; b++) out.PutU32(buckets + b * 4, NONE);

			for (const auto& row : table.rows) {
				const auto fields = out.Alloc(static_cast<uint32_t>(row.size()) * 8);
				for (uint32_t c = 0; c < row.size(); c++) {
					const auto& value = row[c];
					uint32_t raw = 0;
					switch (value.type) {
					case eSqliteDataType::INT32:
					case eSqliteDataType::INT_BOOL:
						raw = static_cast<uint32_t>(static_cast<int32_t>(value.integer));
						break;
					case eSqliteDataType::REAL:
						std::memcpy(&raw, &value.real, sizeof(raw));
						break;
					case eSqliteDataType::INT64:
						raw = out.I64(value.integer);
						break;
					case eSqliteDataType::TEXT_4:
					case eSqliteDataType::TEXT_8:
						raw = out.String(value.text);
						break;
					default:
						break;
					}
					out.PutU32(fields + c * 8, static_cast<uint32_t>(value.type));
					out.PutU32(fields + c * 8 + 4, raw);
				}

				const auto rowData = out.Alloc(8);
				out.PutU32(rowData, static_cast<uint32_t>(row.size()));
				out.PutU32(rowData + 4, fields);

				const auto rowInfo = out.Alloc(8);
				out.PutU32(rowInfo, rowData);
				out.PutU32(rowInfo + 4, NONE);

				// Bucketed by the first column, appended to the end of the bucket's chain
				const auto bucket = static_cast<uint32_t>(static_cast<uint64_t>(row[0].integer) % table.bucketCount);
				if (lastInBucket[bucket] == NONE) out.PutU32(buckets + bucket * 4, rowInfo);
				else out.PutU32(lastInBucket[bucket] + 4, rowInfo);
				lastInBucket[bucket] = rowInfo;
			}

			out.PutU32(headers + t * 8, columnHeader);
			out.PutU32(headers + t * 8 + 4, rowTop);
		}
		return out.Bytes();
	}

	inline void WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
};

#endif // FDBTESTWRITER_H
