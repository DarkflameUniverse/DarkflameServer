# In-game commands

* All commands are prefixed by `/` and typed in the in-game chat window. Some commands require elevated gmlevel privileges. Operands within `<>` are required, operands within `()` are optional.

## General Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|help|`/help (command/page)`|If a command is given, display detailed info on that command. Otherwise display a list of commands with short descriptions. Aliases: `/h`.|0|
|credits|`/credits`|Displays the names of the people behind Darkflame Universe.|0|
|die|`/die`|Smashes the player as if they were killed by something|0|
|info|`/info`|Displays server info to the user, including where to find the server's source code|0|
|instanceinfo|`/instanceinfo`|Display LWOZoneID info for the current zone|0|
|ping|`/ping (-l)`|Displays your average ping. If the `-l` flag is used, the latest ping is displayed.|0|
|pvp|`/pvp`|Toggle your PVP flag|0|
|requestmailcount|`/requestmailcount`|Sends notification with number of unread messages in the player's mailbox. Aliases: `/checkmail`.|0|
|who|`/who`|Displays all players on the instance|0|
|togglenameplate|`/togglenameplate`|Turns the nameplate above your head that is visible to other players off and on. This must be enabled by a server admin. Aliases: `/tnp`.|0|
|toggleskipcinematics|`/toggleskipcinematics`|Skips mission and world load related cinematics. This must be enabled by a server admin. Aliases: `/tsc`.|0|

## Client-handled Commands

These commands are registered by the server so they appear in help, but their behavior is handled by the LEGO Universe client.

### User-Client related
|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|faq|`/faq`|Show the LU FAQ Page. Aliases: `/faqs`.|0|
|camp|`/camp`|Returns you to the character select screen. Aliases: `/logoutcharacter`.|0|
|cancelqueue|`/cancelqueue`|Cancel Your position in the queue if you are in one.|0|
|exit|`/exit`|Exit to desktop. Aliases: `/quit`.|0|
|forums|`/forums`|Show the LU Forums!|0|
|loc|`/loc`|Output your current location on the map to the chat box. Aliases: `/locate`, `/location`.|0|
|logout|`/logout`|Returns you to the login screen. Aliases: `/logoutaccount`.|0|
|minigames|`/minigames`|Show the LEGO minigames page!|0|
|shop|`/shop`|Show the LEGO shop page. Aliases: `/store`.|0|
|perfoptionslow|`/perfoptionslow`|Sets the default low-spec performance options in the cfg file|0|
|perfoptionsmid|`/perfoptionsmid`|Sets the default medium-spec performance options in the cfg file|0|
|perfoptionshigh|`/perfoptionshigh`|Sets the default high-spec performance options in the cfg file|0|
|recommendedperfoptions|`/recommendedperfoptions`|Sets the recommended performance options in the cfg file|0|


### Friend-Team related
|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|addfriend|`/addfriend <name>`|[name] Add a player to your friends list.|0|
|addignore|`/addignore <name>`|[name] Add a player to your ignore list.|0|
|invite|`/invite <name>`|[name] Invite a player to your team. Aliases: `/inviteteam`, `/teaminvite`, `/tinvite`.|0|
|kickplayer|`/kickplayer <name>`|Kicks a player from your current team. The `/kick` alias is reserved by the moderation command. Aliases: `/teamkickplayer`, `/tkick`, `/tkickplayer`.|0|
|leader|`/leader <name>`|[name] Set the leader for your current team. Aliases: `/setleader`, `/teamsetleader`, `/tleader`, `/tsetleader`.|0|
|leave|`/leave`|Leave your current team. Aliases: `/leaveteam`, `/teamleave`, `/tleave`.|0|
|removefriend|`/removefriend <name>`|[name] Removes a player from your friends list.|0|
|removeIgnore|`/removeIgnore <name>`|[name] Removes a player from your ignore list.|0|
|setloot|`/setloot <rr|ffa>`|[rr\|ffa] Set the loot for your current team (round-robin/free for all). Aliases: `/teamsetloot`, `/tloot`, `/tsetloot`.|0|
|s|`/s <message>`|Say something outloud so that everyone can hear you. Aliases: `/say`.|0|
|team|`/team <message>`|Send a message to your teammates. Aliases: `/t`.|0|
|tell|`/tell <name> <message>`|Send a private message to another player. Aliases: `/w`, `/whisper`.|0|


### Actions
|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|yes|`/yes`|Aye aye, captain!|0|
|sigh|`/sigh`|Another day, another brick.|0|
|shrug|`/shrug`|I dunno...|0|
|talk|`/talk`|Jibber Jabber|0|
|thumb|`/thumb`|Oh, yeah!. Aliases: `/thumbs`, `/thumbsup`.|0|
|cringe|`/cringe`|I don't even want to talk about it...|0|
|victory!|`/victory!`|Victory!|0|
|backflip|`/backflip`|Do a flip!|0|
|clap|`/clap`|A round of applause!|0|
|thanks|`/thanks`|Express your gratitude for another.|0|
|wave|`/wave`|Wave to other players.|0|
|why|`/why`|Why\|!?!!|0|
|gasp|`/gasp`|Oh my goodness!|0|
|cry|`/cry`|Show everyone your 'Aw' face.|0|
|dance|`/dance`|Dance 'til you can't dance no more.|0|
|giggle|`/giggle`|A good little chuckle|0|
|salute|`/salute`|For those about to build...|0|

## Moderation Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|gmlevel|`/gmlevel <level>`|Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands. Aliases: `/makegm`, `/setgmlevel`.|0; account GM level must be greater than 0|
|kick|`/kick <username>`|Kicks the player off the server|2|
|mailitem|`/mailitem <player name> <item id>`|Mails an item to the given player. The mailed item has predetermined content. The sender name is set to "Darkflame Universe". The title of the message is "Lost item". The body of the message is "This is a replacement item for one you lost".|3|
|ban|`/ban <username>`|Bans a user from the server|4|
|approveproperty|`/approveproperty`|Approves the property the player is currently visiting|5|
|mute|`/mute <username> (days) (hours)`|Mute player for the given amount of time. If no time is given, the mute is indefinite.|6|
|fly|`/fly <speed>`|Toggles your flying state with an optional parameter for the speed scale.|8|
|attackimmune|`/attackimmune <value>`|Sets the character's immunity to basic attacks state, where value can be one of "1", to make yourself immune to basic attack damage, or "0" to undo|8|
|gmimmune|`/gmimmune <value>`|Sets the character's GMImmune state, where value can be one of "1", to make yourself immune to damage, or "0" to undo|8|
|gminvis|`/gminvis`|Toggles invisibility for the character, though it's currently a bit buggy. Requires nonzero GM Level for the character, but the account must have a GM level of 8.|8|
|setname|`/setname <name>`|Sets a temporary name for your player. The name resets when you log out|8|
|title|`/title <title>`|Temporarily appends your player's name with " - &#60;title&#62;". This resets when you log out|8|
|showall|`/showall (displayZoneData) (displayIndividualPlayers)`|Usage: /showall (displayZoneData: Default 1) (displayIndividualPlayers: Default 1)|2|
|findplayer|`/findplayer <player name>`|Find the World Server a player is in if they are online|2|
|spectate|`/spectate (player name)`|Specify a player name to spectate. They must be in the same world as you. Leave blank to stop spectating. Aliases: `/follow`.|2|

## Server Operation Commands

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|announce|`/announce`|Sends an announcement. `/setanntitle` and `/setannmsg` must be called first to configure the announcement.|8|
|kill|`/kill <username>`|Smashes the character whom the given user is playing|8|
|metrics|`/metrics`|Prints some information about the server's performance|8|
|setannmsg|`/setannmsg <message>`|Sets the message of an announcement. Use with `/setannmtitle` and `/announce`|8|
|setanntitle|`/setanntitle <title>`|Sets the title of an announcement. Use with `/setannmsg` and `/announce`|8|
|shutdown|`/shutdown`|Shuts this world down|8|
|shutdownuniverse|`/shutdownuniverse`|Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.|9|
|uptime|`/uptime`|Display the time the current world server has been active|8|

## Development Commands

These commands are primarily for development and testing. The usage of many of these commands relies on knowledge of the codebase and client SQLite database.

|Command|Usage|Description|Admin Level Requirement|
|--- |--- |--- |--- |
|fix-stats|`/fix-stats`|Resets skills, buffs, and destroyables|0|
|join|`/join <password>`|Join a private zone with given password|0|
|leave-zone|`/leave-zone`|If you are in an instanced zone, transfers you to the closest main world. For example, if you are in an instance of Avant Gardens Survival or the Spider Queen Battle, you are sent to Avant Gardens. If you are in the Battle of Nimbus Station, you are sent to Nimbus Station. Aliases: `/leavezone`.|0|
|resurrect|`/resurrect`|Resurrects the player.|8|
|setminifig|`/setminifig <body part> <minifig item id>`|Alters your player's minifig. Body part can be one of "Eyebrows", "Eyes", "HairColor", "HairStyle", "Pants", "LeftHand", "Mouth", "RightHand", "Shirt", or "Hands". Changing minifig parts could break the character so this command is limited to GMs.|1|
|testmap|`/testmap <zone> (force) (clone-id)`|Transfers you to the given zone by id and clone id. Add "force" to skip checking if the zone is accessible (this can softlock your character, though, if you e.g. try to teleport to Frostburgh). Aliases: `/tm`.|1|
|reportproxphys|`/reportproxphys`|Prints to console the position and radius of proximity sensors.|9|
|spawnphysicsverts|`/spawnphysicsverts`|Spawns a 1x1 brick at all vertices of phantom physics objects|8|
|teleport|`/teleport <x/source player> (y) <z/target player>`|Teleports you. If no Y is given, you are teleported to the height of the terrain or physics object at (x, z). Any of the coordinates can use the syntax of an exact position (10.0), or a relative position (~+10.0). A ~ means use the current value of that axis as the base value. Addition or subtraction is supported (~+10) (~-10). If source player and target player are players that exist in the world, then the source player will be teleported to target player. Aliases: `/tele`, `/tp`.|6|
|activatespawner|`/activatespawner <spawner name>`|Activates spawner by name|8|
|addmission|`/addmission <mission id>`|Accepts the mission, adding it to your journal.|8|
|boost|`/boost (time)`|Adds a passive boost action if you are in a vehicle. If time is given it will end after that amount of time|8|
|unboost|`/unboost`|Removes a passive vehicle boost|8|
|buff|`/buff <id> <duration>`|Applies a buff with the given id for the given number of seconds|8|
|buffme|`/buffme`|Sets health, armor, and imagination to 999|8|
|buffmed|`/buffmed`|Sets health, armor, and imagination to 9|8|
|clearflag|`/clearflag <flag id>`|Removes the given health or inventory flag from your player. Equivalent of calling `/setflag off <flag id>`|8|
|completemission|`/completemission <mission id>`|Completes the mission, removing it from your journal|8|
|createprivate|`/createprivate <zone id> <clone id> <password>`|Creates a private zone with password|8|
|debugui|`/debugui`|Toggle Debug UI|8|
|dismount|`/dismount`|Dismounts you from the vehicle or mount|8|
|reloadconfig|`/reloadconfig`|Reloads the server with the new config values. Aliases: `/reload-config`.|8|
|forcesave|`/forcesave`|While saving to database usually happens on regular intervals and when you disconnect from the server, this command saves your player's data to the database. Aliases: `/force-save`.|8|
|freecam|`/freecam`|Toggles freecam mode|8|
|freemoney|`/freemoney <coins>`|Give yourself coins. Aliases: `/givemoney`, `/money`, `/givecoins`, `/coins`.|8|
|getnavmeshheight|`/getnavmeshheight`|Display the navmesh height at your current position|8|
|giveuscore|`/giveuscore <uscore>`|Gives uscore|8|
|gmadditem|`/gmadditem <id> (count)`|Adds the given item to your inventory by id. Aliases: `/give`.|8|
|inspect|`/inspect <component or ldf variable or player name> (-m <waypoint> | -a <animation> | -s | -p | -f (faction) | -t)`|Finds the closest entity with the given component or LNV variable (ignoring players and racing cars), printing its ID, distance from the player, and whether it is sleeping, as well as the the IDs of all components the entity has. See detailed usage in the DLU docs|8|
|list-spawns|`/list-spawns`|Lists all the character spawn points in the zone. Additionally, this command will display the current scene that plays when the character lands in the next zone, if there is one. Aliases: `/listspawns`.|8|
|locrow|`/locrow`|Prints the your current position and rotation information to the console|8|
|lookup|`/lookup <query>`|Searches through the Objects table in the client SQLite database for items whose display name, name, or description contains the query. Query can be multiple words delimited by spaces.|8|
|playanimation|`/playanimation <id>`|Play an animation with given ID. Aliases: `/playanim`.|8|
|playeffect|`/playeffect <effect id> <effect type> <effect name>`|Plays an effect|8|
|playlvlfx|`/playlvlfx`|Plays the level up animation on your character|8|
|playrebuildfx|`/playrebuildfx`|Plays the quickbuild animation on your character|8|
|pos|`/pos`|Displays your current position in chat and in the console|8|
|refillstats|`/refillstats`|Refills health, armor, and imagination to their maximum level|8|
|reforge|`/reforge <base item id> <reforged item id>`|Reforges an item|8|
|resetmission|`/resetmission <mission id>`|Sets the state of the mission to accepted but not yet started|8|
|rot|`/rot`|Displays your current rotation in chat and in the console|8|
|runmacro|`/runmacro <macro>`|Runs any command macro found in `./res/macros/`|8|
|setcontrolscheme|`/setcontrolscheme <scheme number>`|Sets the character control scheme to the specified number|8|
|setcurrency|`/setcurrency <coins>`|Sets your coins. Aliases: `/setcoins`.|8|
|setflag|`/setflag (value) <flag id>`|Sets the given inventory or health flag to the given value, where value can be one of "on" or "off". If no value is given, by default this adds the flag to your character (equivalent of calling `/setflag on <flag id>`)|8|
|setinventorysize|`/setinventorysize <size> (inventory)`|Sets your inventory size to the given size. If `inventory` is provided, the number or string will be used to set that inventory to the requested size. Aliases: `/setinvsize`, `/setinvensize`.|8|
|setuistate|`/setuistate <ui state>`|Changes UI state|8|
|spawn|`/spawn <id>`|Spawns an object at your location by id|8|
|spawngroup|`/spawngroup <id> <amount> <radius>`|Spawns `<amount>` of object `<id>` within the given `<radius>` from your location|8|
|speedboost|`/speedboost <amount>`|Sets the speed multiplier to the given amount. `/speedboost 1.5` will set the speed multiplier to 1.5x the normal speed|8|
|startcelebration|`/startcelebration <id>`|Starts a celebration effect on your character|8|
|stopeffect|`/stopeffect <effect id>`|Stops the given effect|8|
|toggle|`/toggle <ui state>`|Toggles UI state|8|
|tpall|`/tpall`|Teleports all characters to your current position|8|
|triggerspawner|`/triggerspawner <spawner name>`|Triggers spawner by name|8|
|unlock-emote|`/unlock-emote <emote id>`|Unlocks for your character the emote of the given id. Aliases: `/unlockemote`.|8|
|setlevel|`/setlevel <requested_level> (username)`|Sets the using entities level to the requested level.  Takes an optional parameter of an in-game players username to set the level of|8|
|setskillslot|`/setskillslot <slot> <skill id>`|Set an action slot to a specific skill|8|
|setfaction|`/setfaction <faction id>`|Clears the users current factions and sets it|8|
|addfaction|`/addfaction <faction id>`|Add the faction to the users list of factions|8|
|getfactions|`/getfactions`|Shows the player's factions|8|
|setrewardcode|`/setrewardcode <code>`|Sets the rewardcode for the account you are logged into if it's a valid rewardcode, See cdclient table `RewardCodes`|8|
|barfight|`/barfight`|Starts a barfight (turns everyones pvp on)|8|
|despawn|`/despawn <object id>`|Despawns an object by id|8|
|execute|`/execute <subcommand> ... run <command>`|Execute commands as different entities or from different positions. Usage: /execute <subcommand> ... run <command>. Subcommands: as <entity>, at <entity>, positioned <x> <y> <z>. Aliases: `/exec`.|8|
|crash|`/crash`|Crashes the server. Aliases: `/pumpkin`.|9|
|rollloot|`/rollloot <loot matrix index> <item id> <amount>`|Given a `loot matrix index`, look for `item id` in that matrix `amount` times and print to the chat box statistics of rolling that loot matrix. Aliases: `/roll-loot`.|8|
|castskill|`/castskill <skill id>`|Casts the skill as the player|8|
|deleteinven|`/deleteinven <inventory>`|Delete all items from a specified inventory|8|

## Detailed `/inspect` Usage

`/inspect <component> (-m <waypoint> | -a <animation> | -p | -f (faction) | -t)`

Finds the closest entity with the given component or LDF variable (ignoring players and racing cars), printing its ID, distance from the player, and whether it is sleeping, as well as the the IDs of all components the entity has.
This info is then shown in a window on the client which the user can navigate

### `/inspect` Options

* `-m`: If the entity has a moving platform component, sends it to the given waypoint, or stops the platform if `waypoint` is `-1`.
* `-a`: Plays the given animation on the entity.
* `-p`: Prints the entity's position
* `-f`: If the entity has a destroyable component, prints whether the entity is smashable and its friendly and enemy faction IDs; if `faction` is specified, adds that faction to the entity.
* `-cf`: check if the entity is enemy or friend
* `-t`: If the entity has a phantom physics component, prints the effect type, direction, directional multiplier, and whether the effect is active; in any case, if the entity has a trigger, prints the trigger ID.

## Game Master Levels

There are 9 Game master levels

|Level|Variable Name|Description|
|--- |--- |--- |
|0|CIVILIAN|Normal player|
|1|FORUM_MODERATOR|Forum moderator. No permissions on live servers.|
|2|JUNIOR_MODERATOR|Can kick/mute and pull chat logs|
|3|MODERATOR|Can return lost items|
|4|SENIOR_MODERATOR|Can ban|
|5|LEAD_MODERATOR|Can approve properties|
|6|JUNIOR_DEVELOPER|Junior developer & future content team. Civilan on live.|
|7|INACTIVE_DEVELOPER|Inactive developer, limited permissions.|
|8|DEVELOPER|Active developer, full permissions on live.|
|9|OPERATOR|Can shutdown server for restarts & updates.|
