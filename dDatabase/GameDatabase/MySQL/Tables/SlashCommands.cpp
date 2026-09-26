#include "MySQLDatabase.h"

std::vector<ISlashCommands::SlashCommand> MySQLDatabase::GetSlashCommands() {
	std::vector<SlashCommand> commands;
	auto result = ExecuteSelect("SELECT * FROM slash_commands ORDER BY name;");
	const auto text = [&](const char* field) { return std::string(result->getString(field).c_str()); };
	while (result->next()) {
		SlashCommand command;
		command.name = text("name");
		command.aliases = GeneralUtils::SplitString(text("aliases"), ',');
		command.help = text("help");
		command.info = text("info");
		command.defaultLevel = static_cast<uint8_t>(result->getInt("default_level"));
		command.minLevel = static_cast<uint8_t>(result->getInt("min_level"));
		command.fixed = result->getInt("fixed") != 0;
		command.clientHandled = result->getInt("client_handled") != 0;
		command.note = text("note");
		command.dashboardPermission = text("dashboard_permission");
		command.targetRule = text("target_rule");
		command.followsPermission = result->getInt("follows_permission") != 0;
		commands.push_back(std::move(command));
	}
	return commands;
}

void MySQLDatabase::SetSlashCommand(const SlashCommand& command) {
	std::string aliases;
	for (const auto& alias : command.aliases) aliases += (aliases.empty() ? "" : ",") + alias;
	ExecuteInsert(
		"INSERT INTO slash_commands (name, aliases, help, info, default_level, min_level, fixed, client_handled, note, dashboard_permission, target_rule, follows_permission) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE aliases = VALUES(aliases), help = VALUES(help), info = VALUES(info), default_level = VALUES(default_level), "
		"min_level = VALUES(min_level), fixed = VALUES(fixed), client_handled = VALUES(client_handled), note = VALUES(note), dashboard_permission = VALUES(dashboard_permission), target_rule = VALUES(target_rule), follows_permission = VALUES(follows_permission);",
		command.name, aliases, command.help, command.info, static_cast<int32_t>(command.defaultLevel),
		static_cast<int32_t>(command.minLevel), command.fixed, command.clientHandled, command.note, command.dashboardPermission, command.targetRule, command.followsPermission);
}

void MySQLDatabase::DeleteSlashCommand(const std::string& name) {
	ExecuteDelete("DELETE FROM slash_commands WHERE name = ?;", name);
}
