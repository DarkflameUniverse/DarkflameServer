#include "SQLiteDatabase.h"

std::vector<ISlashCommands::SlashCommand> SQLiteDatabase::GetSlashCommands() {
	std::vector<SlashCommand> commands;
	auto [_, result] = ExecuteSelect("SELECT * FROM slash_commands ORDER BY name;");
	while (!result.eof()) {
		SlashCommand command;
		command.name = result.getStringField("name");
		command.aliases = GeneralUtils::SplitString(result.getStringField("aliases"), ',');
		command.help = result.getStringField("help");
		command.info = result.getStringField("info");
		command.defaultLevel = static_cast<uint8_t>(result.getIntField("default_level"));
		command.minLevel = static_cast<uint8_t>(result.getIntField("min_level"));
		command.fixed = result.getIntField("fixed") != 0;
		command.clientHandled = result.getIntField("client_handled") != 0;
		command.note = result.getStringField("note");
		command.dashboardPermission = result.getStringField("dashboard_permission");
		command.targetRule = result.getStringField("target_rule");
		command.followsPermission = result.getIntField("follows_permission") != 0;
		commands.push_back(std::move(command));
		result.nextRow();
	}
	return commands;
}

void SQLiteDatabase::SetSlashCommand(const SlashCommand& command) {
	std::string aliases;
	for (const auto& alias : command.aliases) aliases += (aliases.empty() ? "" : ",") + alias;
	ExecuteInsert(
		"INSERT INTO slash_commands (name, aliases, help, info, default_level, min_level, fixed, client_handled, note, dashboard_permission, target_rule, follows_permission) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(name) DO UPDATE SET aliases = excluded.aliases, help = excluded.help, info = excluded.info, default_level = excluded.default_level, "
		"min_level = excluded.min_level, fixed = excluded.fixed, client_handled = excluded.client_handled, note = excluded.note, dashboard_permission = excluded.dashboard_permission, target_rule = excluded.target_rule, follows_permission = excluded.follows_permission;",
		command.name, aliases, command.help, command.info, static_cast<int32_t>(command.defaultLevel),
		static_cast<int32_t>(command.minLevel), command.fixed, command.clientHandled, command.note, command.dashboardPermission, command.targetRule, command.followsPermission);
}

void SQLiteDatabase::DeleteSlashCommand(const std::string& name) {
	ExecuteDelete("DELETE FROM slash_commands WHERE name = ?;", name);
}
