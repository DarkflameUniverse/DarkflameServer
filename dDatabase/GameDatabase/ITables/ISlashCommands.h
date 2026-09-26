#ifndef __ISLASHCOMMANDS__H__
#define __ISLASHCOMMANDS__H__

#include <cstdint>
#include <string>
#include <vector>

/**
 * The in-game slash commands, as the world servers registered them, so the dashboard can list them and change who may
 * use them (the levels themselves are settings: command_level_<name>, see SlashCommandLevels.h).
 */
class ISlashCommands {
public:
	struct SlashCommand {
		std::string name;                 // settings name, from the first alias
		std::vector<std::string> aliases; // what players type, first one first
		std::string help;
		std::string info;
		uint8_t defaultLevel{};           // the level in the code
		uint8_t minLevel{};               // the lowest level it may be set to
		bool fixed{};                     // always defaultLevel
		bool clientHandled{};             // the game client acts on it by itself (emotes, team and friend commands)
		std::string note;                 // why it has a floor or is fixed
		std::string dashboardPermission;  // the dashboard permission that does the same thing, if any (its level is the command's)
		std::string targetRule;           // who it may be used on: SlashCommandLevels::TargetRuleName ("" acts on nobody else)
		bool followsPermission{};         // uses dashboardPermission's level (false in rows stored before commands did)

		bool operator==(const SlashCommand&) const = default;
	};

	// Every command, by name
	virtual std::vector<SlashCommand> GetSlashCommands() = 0;

	// Add or update one command
	virtual void SetSlashCommand(const SlashCommand& command) = 0;

	// Forget a command the code no longer has
	virtual void DeleteSlashCommand(const std::string& name) = 0;
};

#endif  //!__ISLASHCOMMANDS__H__
