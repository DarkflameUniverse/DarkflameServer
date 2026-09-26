#pragma once

#include <cctype>
#include <string>
#include <string_view>

#include "json.hpp"
#include "magic_enum.hpp"
#include "eGameMasterLevel.h"
#include "eInventoryType.h"
#include "ePropertyPrivacyOption.h"
#include "IDashboardAdmin.h"
#include "IEconomyLedger.h"
#include "IScheduledTasks.h"

/**
 * Names for the game's numbered values (GM levels, inventories, property privacy), built from the server's
 * own enums so the pages never keep their own copies. Every page gets them (RenderPage: `labels` for templates,
 * <body data-labels> for scripts, read with Labels.name(kind, value) in common.js).
 */
namespace GameLabels {
	// FORUM_MODERATOR -> "Forum Moderator", SmashablesSmashed -> "Smashables Smashed"
	inline std::string Words(std::string_view name) {
		std::string text;
		bool start = true;
		for (size_t i = 0; i < name.size(); i++) {
			const char c = name[i];
			if (c == '_') { text += ' '; start = true; continue; }
			// A capital after a small letter starts a new word in CamelCase names
			if (i > 0 && std::isupper(static_cast<unsigned char>(c)) && std::islower(static_cast<unsigned char>(name[i - 1]))) { text += ' '; start = true; }
			text += start ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			start = false;
		}
		return text;
	}

	// Any enum value's name as words: VAULT_MODELS -> "Vault Models"
	template<typename Enum> std::string Name(Enum value) {
		return Words(magic_enum::enum_name(value));
	}

	// Every value of an enum as [{value, name}]
	template<typename Enum> nlohmann::json List() {
		nlohmann::json list = nlohmann::json::array();
		for (const auto value : magic_enum::enum_values<Enum>()) list.push_back({ {"value", static_cast<int>(value)}, {"name", Name(value)} });
		return list;
	}

	// {gmLevels: [{value, name}], inventories, privacy, transferMethods, flagKinds, flagStatus, taskStatus}
	inline const nlohmann::json& Json() {
		static const nlohmann::json labels = [] {
			nlohmann::json json{ {"gmLevels", nlohmann::json::array()}, {"inventories", nlohmann::json::array()},
				{"privacy", nlohmann::json::array()} };
			for (const auto level : magic_enum::enum_values<eGameMasterLevel>()) {
				json["gmLevels"].push_back({ {"value", static_cast<int>(level)}, {"name", Name(level)} });
			}
			for (const auto type : magic_enum::enum_values<eInventoryType>()) {
				if (type == INVALID || type == ALL) continue; // not real inventories
				json["inventories"].push_back({ {"value", static_cast<int>(type)}, {"name", Name(type)} });
			}
			for (const auto option : magic_enum::enum_values<PropertyPrivacyOption>()) {
				json["privacy"].push_back({ {"value", static_cast<int>(option)}, {"name", Name(option)} });
			}
			json["transferMethods"] = List<IEconomyLedger::eTransferMethod>();
			json["flagKinds"] = List<IDashboardAdmin::eFlagKind>();
			json["flagStatus"] = List<IDashboardAdmin::eFlagStatus>();
			json["taskStatus"] = List<IScheduledTasks::eRunStatus>();
			return json;
		}();
		return labels;
	}
}
