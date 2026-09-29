#include "ClientSysInfo.h"

#include <array>
#include <cctype>
#include <charconv>

namespace {
	struct Reader {
		std::string_view text;
		size_t pos{};

		void SkipSpace() {
			while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) pos++;
		}

		bool Literal(std::string_view literal) {
			SkipSpace();
			if (text.substr(pos, literal.size()) != literal) return false;
			pos += literal.size();
			return true;
		}

		std::optional<int64_t> Number() {
			SkipSpace();
			int64_t value{};
			const auto* begin = text.data() + pos;
			const auto [end, error] = std::from_chars(begin, text.data() + text.size(), value);
			if (error != std::errc{} || end == begin) return std::nullopt;
			pos += static_cast<size_t>(end - begin);
			return value;
		}
	};
}

ClientSysInfo::MemoryStats ClientSysInfo::ParseMemoryStats(const std::string_view text) {
	MemoryStats stats;
	// Each part: text before the number, text after it, and where the number goes
	struct Part { std::string_view prefix; std::string_view suffix; std::optional<int64_t> MemoryStats::* field; };
	const std::array<Part, 11> parts{ {
		{ "", "p,", &MemoryStats::workingSetBytes },
		{ "", "vbytes.", &MemoryStats::pagefileUsageBytes },
		{ "", "n-use.", &MemoryStats::memoryLoadPercent },
		{ "", "TKb-pmem.", &MemoryStats::totalPhysKb },
		{ "", "FKb pmem.", &MemoryStats::availPhysKb },
		{ "", "TKb pfile.", &MemoryStats::totalPageFileKb },
		{ "", "FKb pfile.", &MemoryStats::availPageFileKb },
		{ "", "TKbytes vmem.", &MemoryStats::totalVirtualKb },
		{ "", "FKb vmem.", &MemoryStats::availVirtualKb },
		{ "P", "p,", &MemoryStats::peakWorkingSetBytes },
		{ "", "v.", &MemoryStats::peakPagefileUsageBytes },
	} };
	Reader reader{ text };
	for (const auto& part : parts) {
		if (!part.prefix.empty() && !reader.Literal(part.prefix)) return stats;
		const auto value = reader.Number();
		if (!value || !reader.Literal(part.suffix)) return stats;
		stats.*part.field = *value;
	}
	stats.complete = true;
	return stats;
}
