#ifndef __BBBAUTOSAVEITEMS__H__
#define __BBBAUTOSAVEITEMS__H__

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "dCommonVars.h"

// bbb_autosave.source_items: the item ids as a comma separated list
namespace BbbAutosaveItems {
	inline std::string Join(const std::vector<LWOOBJID>& items) {
		std::string out;
		for (const auto id : items) {
			if (!out.empty()) out += ',';
			out += std::to_string(id);
		}
		return out;
	}

	inline std::vector<LWOOBJID> Parse(const std::string_view text) {
		std::vector<LWOOBJID> items;
		size_t start = 0;
		while (start < text.size()) {
			auto end = text.find(',', start);
			if (end == std::string_view::npos) end = text.size();
			const std::string part(text.substr(start, end - start));
			char* parsedEnd = nullptr;
			const auto id = std::strtoll(part.c_str(), &parsedEnd, 10);
			if (parsedEnd != part.c_str() && id != 0) items.push_back(id);
			start = end + 1;
		}
		return items;
	}
}

#endif //!__BBBAUTOSAVEITEMS__H__
