# In-game commands

Here is a summary of the commands available in-game. All commands are prefixed by `/` and typed in the in-game chat window. Some commands requires admin privileges. Operands within `<>` are required, operands within `()` are not. For the full list of in-game commands, please checkout [the source file](../dGame/dUtilities/SlashCommandHandler.cpp).

## General Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|credits|`/credits`|Displays the names of the people behind Darkflame Universe.||
|die|`/die`|Smashes the player.||
|info|`/info`|Displays server info to the user, including where to find the server's source code.||
|instanceinfo|`/instanceinfo`|Displays in the chat the current zone, clone, and instance id.||
|ping|`/ping (-l)`|Displays in chat your average ping. If the `-l` flag is used, the latest ping is displayed.||
|pvp|`/pvp`|Toggle your PVP flag.||
|resurrect|`/resurrect`|Resurrects the player.||
|requestmailcount|`/requestmailcount`|Sends notification with number of unread messages in the player's mailbox.||
|skip-ags|`/skip-ags`|Skips the Avant Gardens Survival minigame mission.||
|skip-sg|`/skip-sg`|Skips the Shooting Gallery minigame mission.||
|who|`/who`|Displays in chat all players on the instance.||

## Moderation Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|gmlevel|`/gmlevel <level>`|Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands.||
|kick|`/kick <username>`|Kicks the player off the server.|2|
|mute|`/mute <username> (days) (hours)`|Mute player for the given amount of time. If no time is given, the mute is indefinite.|2|
|ban|`/ban <username>`|Bans a user from the server.|4|
|approveproperty|`/approveproperty`|Approves the property the player is currently visiting.|5|
|mailitem|`/mailitem <player name> <item id>`|Mails an item to the given player. The mailed item has predetermined content. The sender name is set to "Darkflame Universe." The title of the message is "Lost item." The body of the message is "This is a replacement item for one you lost."|8|
|setname|`/setname <name>`|Sets a temporary name for your player. The name resets when you log out.|8|
|title|`/title <title>`|Temporarily appends your player's name with " - &#60;title&#62;". This resets when you log out.|8|

## Server Operation Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|announce|`/announce`|Sends a announcement. `/setanntitle` and `/setannmsg` must be called first to configure the announcement.|8|
|config-set|`/config-set <key> <value>`|Set configuration item.|8|
|config-get|`/config-get <key>`|Get current value of a configuration item.|8|
|kill|`/kill <username>`|Smashes the character whom the given user is playing.|8|
|metrics|`/metrics`|Prints some information about the server's performance.|8|
|setannmsg|`/setannmsg <title>`|Sets the message of an announcment.|8|
|setanntitle|`/setanntitle <title>`|Sets the title of an announcment.|8|
|shutdownuniverse|`/shutdownuniverse`|Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.|8|

## Development Commands

These commands are primarily for development and testing. The usage of many of these commands relies on knowledge of the codebase and client SQLite database.

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|fix-stats|`/fix-stats`|Resets skills, buffs, and destroyables.||
|join|`/join <password>`|Joins a private zone with given password.||
|leave-zone|`/leave-zone`|Transfers to Avant Gardens. If you are in the Battle of Nimbus Station, you are sent to Nimbus station.||
|playanimation|`/playanimation <id>`|Plays animation with given ID.||
|setminifig|`/setminifig <body part> <minifig item id>`|Alters your player's minifig. Body part can be one of "Eyebrows", "Eyes", "HairColor", "HairStyle", "Pants", "LeftHand", "Mouth", "RightHand", "Shirt", or "Hands". Changing minifig parts could break the character so this command is limited to GMs.|1|
|testmap|`/testmap <zone> (clone-id)`|Transfers you to the given zone by id and clone id.|1|
|spawnphysicsverts|`/spawnphysicsverts`|Spawns a 1x1 brick at all vertices of phantom physics objects.|6|
|reportproxphys|`/reportproxphys`|Prints to console the position and radius of proximity sensors.|6|
|activatespawner|`/activatespawner <spawner name>`|Activates spawner by name.|8|
|addmission|`/addmission <mission id>`|Accepts the mission, adding it to your journal.|8|
|boost|`/boost`|Adds a passive boost action if you are in a vehicle.|8|
|buff|`/buff <id> <duration>`|Applies the buff with the given id for the given number of seconds.|8|
|buffme|`/buffme`|Sets health, armor, and imagination to 999.|8|
|buffmed|`/buffmed`|Sets health, armor, and imagination to 9.|8|
|clearflag|`/clearflag <flag id>`|Removes the given health or inventory flag from your player.|8|
|completemission|`/completemission <mission id>`|Completes the mission, removing it from your journal.|8|
|createprivate|`/createprivate <zone id> <clone id> <password>`|Creates a private zone with password.|8|
|debugui|`/debugui`|Toggle Debug UI.|8|
|dismount|`/dismount`|Dismounts you from the vehicle.|8|
|force-save|`/force-save`|While saving to database usually happens on regular intervals and when you disconnect from the server, this command saves your player's data to the database.|8|
|freecam|`/freecam`|Toggles freecam mode.|8|
|freemoney|`/freemoney <coins>`|Gives coins.|8|
|getnavmeshheight|`/getnavmeshheight`|Displays the navmesh height at your current position.|8|
|giveuscore|`/giveuscore <uscore>`|Gives uscore.|8|
|gmadditem|`/gmadditem <id> (count)`|Adds the given item to your inventory by id.|8|
|inspect|`/inspect (entity id)`|Inspects closest entity.|8|
|list-spawns|`/list-spawns`|Lists all the character spawn points in the zone. Additionally, this command will display the current scene that plays when the character lands in the next zone, if there is one.|8|
|locrow|`/locrow`|Prints the your current position and rotation information to the console.|8|
|lookup|`/lookup <query>`|Searches through the Objects table in the client SQLite database for items whose display name, name, or discription contains the query.|8|
|playeffect|`/playeffect <effect id> <effect type> <effect name>`|Plays an effect.|8|
|playlvlfx|`/playlvlfx`|Plays the level up animation on your character.|8|
|playrebuildfx|`/playrebuildfx`|Plays the quickbuild animation on your character.|8|
|pos|`/pos`|Displays your current position in chat and in the console.|8|
|refillstats|`/refillstats`|Refills health, armor, and imagination to their maximum level.|8|
|reforge|`/reforge <base item id> <reforged item id>`|Reforges an item.|8|
|resetmission|`/resetmission <mission id>`|Sets the state of the mission to accepted but not yet started.|8|
|rollloot|`/rollloot <loot matrix index> <item id> <amount>`|Rolls loot matrix.|9|
|rot|`/rot`|Displays your current rotation in chat and in the console.|8|
|runmacro|`/runmacro <macro>`|Runs any command macro found in `./res/macros/`|8|
|setcontrolscheme|`/setcontrolscheme <scheme number>`|Sets the character control scheme to the specified number.|8|
|setcurrency|`/setcurrency <coins>`|Sets your coins.|8|
|setflag|`/setflag <flag id>`|Adds the given health or inventory flag to your character.|8|
|setinventorysize|`/setinventorysize <size>`|Sets your inventory size to the given size.|8|
|setuistate|`/setuistate <ui state>`|Changes UI state.|8|
|spawn|`/spawn <id>`|Spawns an object at your location by id.|8|
|speedboost|`/speedboost <amount>`|Sets the speed multiplier to the given amount. `/speedboost 1.5` will set the speed multiplier to 1.5x the normal speed.|8|
|startcelebration|`/startcelebration <id>`|Starts a celebration effect on your character.|8|
|stopeffect|`/stopeffect <effect id>`|Stops the given effect.|8|
|teleport|`/teleport <x> (y) <z>`|Teleports you. If no Y is given, you are teleported to the height of the terrain or physics object at (x, z)|8|
|toggle|`/toggle <ui state>`|Toggles UI state.|8|
|tpall|`/tpall`|Teleports all characters to your current position.|8|
|triggerspawner|`/triggerspawner <spawner name>`|Triggers spawner by name.|8|
|unlock-emote|`/unlock-emote <emote id>`|Unlocks for your character the emote of the given id.|8|
|crash|`/crash`|Crashes the server.|9|

## Game Master Levels

There are 9 Game master levels

|Level|Variable Name|Description|
|--- |--- |--- |
|0|GAME_MASTER_LEVEL_CIVILIAN|Normal player|
|1|GAME_MASTER_LEVEL_FORUM_MODERATOR|Forum moderator. No permissions on live servers.|
|2|GAME_MASTER_LEVEL_JUNIOR_MODERATOR|Can kick/mute and pull chat logs|
|3|GAME_MASTER_LEVEL_MODERATOR|Can return lost items|
|4|GAME_MASTER_LEVEL_SENIOR_MODERATOR|Can ban|
|5|GAME_MASTER_LEVEL_LEAD_MODERATOR|Can approve properties|
|6|GAME_MASTER_LEVEL_JUNIOR_DEVELOPER|Junior developer & future content team. Civilan on live.|
|7|GAME_MASTER_LEVEL_INACTIVE_DEVELOPER|Inactive developer, limited permissions.|
|8|GAME_MASTER_LEVEL_DEVELOPER|Active developer, full permissions on live.|
|9|GAME_MASTER_LEVEL_OPERATOR|Can shutdown server for restarts & updates.|
