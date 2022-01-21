# In-game commands

Here is a summary of the commands available in-game. All commands are prefixed by `/` and typed in the in-game chat window. Some commands requires admin privileges. Operands within `<>` are required, operands within `()` are not. For the full list of in-game commands, please checkout [the source file](../dGame/dUtilities/SlashCommandHandler.cpp).

## General Commands

<table>
<thead>
  <th>
    Command
  </th>
  <th>
    Usage
  </th>
  <th>
    Description
  </th>
  <th>
    Admin Level Requirement
  </th>
</thead>
<tbody>
  <tr>
    <td>
      credits
    </td>
    <td>
      /credits
    </td>
    <td>
      Displays the names of the people behind Darkflame Universe.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      die
    </td>
    <td>
      /die
    </td>
    <td>
      Smashes the player.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      info
    </td>
    <td>
      /info
    </td>
    <td>
      Displays server info to the user, including where to find the server's source code.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      instanceinfo
    </td>
    <td>
      /instanceinfo
    </td>
    <td>
      Displays in the chat the current zone, clone, and instance id.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      ping
    </td>
    <td>
      /ping (-l)
    </td>
    <td>
      Displays in chat your average ping. If the -l flag is used, the latest ping is displayed.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      pvp
    </td>
    <td>
      /pvp
    </td>
    <td>
      Toggle your PVP flag.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      resurrect
    </td>
    <td>
      /resurrect
    </td>
    <td>
      Resurrects the player.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      requestmailcount
    </td>
    <td>
      /requestmailcount
    </td>
    <td>
      Sends notification with number of unread messages in the player's mailbox.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      skip-ags
    </td>
    <td>
      /skip-ags
    </td>
    <td>
      Skips the Avant Gardens Survival minigame mission.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      skip-sg
    </td>
    <td>
      /skip-sg
    </td>
    <td>
      Skips the Shooting Gallery minigame mission.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      who
    </td>
    <td>
      /who
    </td>
    <td>
      Displays in chat all players on the instance.
    </td>
    <td>
    </td>
  </tr>
</tbody>
</table>

## Moderation Commands

<table>
<thead>
  <th>
    Command
  </th>
  <th>
    Usage
  </th>
  <th>
    Description
  </th>
  <th>
    Admin Level Requirement
  </th>
</thead>
<tbody>
  <tr>
    <td>
      gmlevel
    </td>
    <td>
      /gmlevel &#60;level&#62;
    </td>
    <td>
      Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      kick
    </td>
    <td>
      /kick &#60;username&#62;
    </td>
    <td>
      Kicks the player off the server.
    </td>
    <td>
      2
    </td>
  </tr>
  <tr>
    <td>
      mute
    </td>
    <td>
      /mute &#60;username&#62; (days) (hours)
    </td>
    <td>
      Mute player for the given amount of time. If no time is given, the mute is indefinite.
    </td>
    <td>
      2
    </td>
  </tr>
  <tr>
    <td>
      ban
    </td>
    <td>
      /ban &#60;username&#62;
    </td>
    <td>
      Bans a user from the server.
    </td>
    <td>
      4
    </td>
  </tr>
  <tr>
    <td>
      approveproperty
    </td>
    <td>
      /approveproperty
    </td>
    <td>
      Approves the property the player is currently visiting.
    </td>
    <td>
      5
    </td>
  </tr>
   <tr>
    <td>
      mailitem
    </td>
    <td>
      /mailitem &#60;player name&#62; &#60;item id&#62;
    </td>
    <td>
      Mails an item to the given player. The mailed item has predetermined content. The sender name is set to "Darkflame Universe." The title of the message is "Lost item." The body of the message is "This is a replacement item for one you lost."
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setname
    </td>
    <td>
      /setname &#60;name&#62;
    </td>
    <td>
      Sets a temporary name for your player. The name resets when you log out.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      title
    </td>
    <td>
      /title &#60;title&#62;
    </td>
    <td>
      Temporarily appends your player's name with " - &#60;title&#62;". This resets when you log out.
    </td>
    <td>
      8
    </td>
  </tr>
</tbody>
</table>

## Server Operation Commands

<table>
<thead>
  <th>
    Command
  </th>
  <th>
    Usage
  </th>
  <th>
    Description
  </th>
  <th>
    Admin Level Requirement
  </th>
</thead>
<tbody>
  <tr>
    <td>
      announce
    </td>
    <td>
      /announce
    </td>
    <td>
      Sends a announcement. /setanntitle and /setannmsg must be called first to configure the announcement.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      config-set
    </td>
    <td>
      /config-set &#60;key&#62; &#60;value&#62;
    </td>
    <td>
      Set configuration item.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      config-get
    </td>
    <td>
      /config-get &#60;key&#62;
    </td>
    <td>
      Get current value of a configuration item.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      kill
    </td>
    <td>
      /kill &#60;username&#62;
    </td>
    <td>
      Smashes the character whom the given user is playing.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      metrics
    </td>
    <td>
      /metrics
    </td>
    <td>
      Prints some information about the server's performance.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setannmsg
    </td>
    <td>
      /setannmsg &#60;title&#62;
    </td>
    <td>
      Sets the message of an announcment.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setanntitle
    </td>
    <td>
      /setanntitle &#60;title&#62;
    </td>
    <td>
      Sets the title of an announcment.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      shutdownuniverse
    </td>
    <td>
      /shutdownuniverse
    </td>
    <td>
      Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.
    </td>
    <td>
      8
    </td>
  </tr>
</tbody>
</table>

## Development Commands

These commands are primarily for development and testing. The usage of many of these commands relies on knowledge of the codebase and client SQLite database.

<table>
<thead>
  <th>
    Command
  </th>
  <th>
    Usage
  </th>
  <th>
    Description
  </th>
  <th>
    Admin Level Requirement
  </th>
</thead>
<tbody>
  <tr>
    <td>
      fix-stats
    </td>
    <td>
      /fix-stats
    </td>
    <td>
      Resets skills, buffs, and destroyables.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      join
    </td>
    <td>
      /join &#60;password&#62;
    </td>
    <td>
      Joins a private zone with given password.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      leave-zone
    </td>
    <td>
      /leave-zone
    </td>
    <td>
      Transfers to Avant Gardens. If you are in the Battle of Nimbus Station, you are sent to Nimbus station.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      playanimation
    </td>
    <td>
      /playanimation &#60;id&#62;
    </td>
    <td>
      Plays animation with given ID.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      setminifig
    </td>
    <td>
      /setminifig &#60;body part&#62; &#60;minifig item id&#62;
    </td>
    <td>
      Alters your player's minifig. Body part can be one of "Eyebrows", "Eyes", "HairColor", "HairStyle", "Pants", "LeftHand", "Mouth", "RightHand", "Shirt", or "Hands". Changing minifig parts could break the character so this command is limited to GMs.
    </td>
    <td>
      1
    </td>
  </tr>
  <tr>
    <td>
      testmap
    </td>
    <td>
      /testmap &#60;zone&#62; (clone-id)
    </td>
    <td>
      Transfers you to the given zone by id and clone id.
    </td>
    <td>
      1
    </td>
  </tr>
  <tr>
    <td>
      spawnphysicsverts
    </td>
    <td>
      /spawnphysicsverts
    </td>
    <td>
      Spawns a 1x1 brick at all vertices of phantom physics objects.
    </td>
    <td>
      6
    </td>
  </tr>
  <tr>
    <td>
      reportproxphys
    </td>
    <td>
      /reportproxphys
    </td>
    <td>
      Prints to console the position and radius of proximity sensors.
    </td>
    <td>
      6
    </td>
  </tr>
  <tr>
    <td>
      activatespawner
    </td>
    <td>
      /activatespawner &#60;spawner name&#62;
    </td>
    <td>
      Activates spawner by name.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      addmission
    </td>
    <td>
      /addmission &#60;mission id&#62;
    </td>
    <td>
      Accepts the mission, adding it to your journal.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      boost
    </td>
    <td>
      /boost
    </td>
    <td>
      Adds a passive boost action if you are in a vehicle.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      buff
    </td>
    <td>
      /buff &#60;id&#62; &#60;duration&#62;
    </td>
    <td>
      Applies the buff with the given id for the given number of seconds.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      buffme
    </td>
    <td>
      /buffme
    </td>
    <td>
      Sets health, armor, and imagination to 999.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      buffmed
    </td>
    <td>
      /buffmed
    </td>
    <td>
      Sets health, armor, and imagination to 9.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      clearflag
    </td>
    <td>
      /clearflag &#60;flag id&#62;
    </td>
    <td>
      Removes the given health or inventory flag from your player.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      completemission
    </td>
    <td>
      /completemission &#60;mission id&#62;
    </td>
    <td>
      Completes the mission, removing it from your journal.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      createprivate
    </td>
    <td>
      /createprivate &#60;zone id&#62; &#60;clone id&#62; &#60;password&#62;
    </td>
    <td>
      Creates a private zone with password.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      debugui
    </td>
    <td>
      /debugui
    </td>
    <td>
      Toggle Debug UI.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      dismount
    </td>
    <td>
      /dismount
    </td>
    <td>
      Dismounts you from the vehicle.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      force-save
    </td>
    <td>
      /force-save
    </td>
    <td>
      While saving to database usually happens on regular intervals and when you disconnect from the server, this command saves your player's data to the database.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      freecam
    </td>
    <td>
      /freecam
    </td>
    <td>
      Toggles freecam mode.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      freemoney
    </td>
    <td>
      /freemoney &#60;coins&#62;
    </td>
    <td>
      Gives coins.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      getnavmeshheight
    </td>
    <td>
      /getnavmeshheight
    </td>
    <td>
      Displays the navmesh height at your current position.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      giveuscore
    </td>
    <td>
      /giveuscore &#60;uscore&#62;
    </td>
    <td>
      Gives uscore.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      gmadditem
    </td>
    <td>
      /gmadditem &#60;id&#62; (count)
    </td>
    <td>
      Adds the given item to your inventory by id.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      inspect
    </td>
    <td>
      /inspect (entity id)
    </td>
    <td>
      Inspects closest entity.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      list-spawns
    </td>
    <td>
      /list-spawns
    </td>
    <td>
      Lists all the character spawn points in the zone. Additionally, this command will display the current scene that plays when the character lands in the next zone, if there is one.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      locrow
    </td>
    <td>
      /locrow
    </td>
    <td>
      Prints the your current position and rotation information to the console.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      lookup
    </td>
    <td>
      /lookup &#60;query&#62;
    </td>
    <td>
      Searches through the Objects table in the client SQLite database for items whose display name, name, or discription contains the query.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      playeffect
    </td>
    <td>
      /playeffect &#60;effect id&#62; &#60;effect type&#62; &#60;effect name&#62;
    </td>
    <td>
      Plays an effect.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      playlvlfx
    </td>
    <td>
      /playlvlfx
    </td>
    <td>
      Plays the level up animation on your character.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      playrebuildfx
    </td>
    <td>
      /playrebuildfx
    </td>
    <td>
      Plays the quickbuild animation on your character.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      pos
    </td>
    <td>
      /pos
    </td>
    <td>
      Displays your current position in chat and in the console.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      refillstats
    </td>
    <td>
      /refillstats
    </td>
    <td>
      Refills health, armor, and imagination to their maximum level.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      reforge
    </td>
    <td>
      /reforge &#60;base item id&#62; &#60;reforged item id&#62;
    </td>
    <td>
      Reforges an item.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      resetmission
    </td>
    <td>
      /resetmission &#60;mission id&#62;
    </td>
    <td>
      Sets the state of the mission to accepted but not yet started.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      rollloot
    </td>
    <td>
      /rollloot &#60;loot matrix index&#62; &#60;item id&#62; &#60;amount&#62;
    </td>
    <td>
      Rolls loot matrix.
    </td>
    <td>
      9
    </td>
  </tr>
  <tr>
    <td>
      rot
    </td>
    <td>
      /rot
    </td>
    <td>
      Displays your current rotation in chat and in the console.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      runmacro
    </td>
    <td>
      /runmacro &#60;macro&#62;
    </td>
    <td>
      Runs any macro found in ./res/macros/
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setcontrolscheme
    </td>
    <td>
      /setcontrolscheme &#60;scheme number&#62;
    </td>
    <td>
      Sets the character control scheme to the specified number.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setcurrency
    </td>
    <td>
      /setcurrency &#60;coins&#62;
    </td>
    <td>
      Sets your coins.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setflag
    </td>
    <td>
      /setflag &#60;flag id&#62;
    </td>
    <td>
      Adds the given health or inventory flag to your character.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setinventorysize
    </td>
    <td>
      /setinventorysize &#60;size&#62;
    </td>
    <td>
      Sets your inventory size to the given size.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      setuistate
    </td>
    <td>
      /setuistate &#60;ui state&#62;
    </td>
    <td>
      Changes UI state.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      spawn
    </td>
    <td>
      /spawn &#60;id&#62;
    </td>
    <td>
      Spawns an object at your location by id.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      speedboost
    </td>
    <td>
      /speedboost &#60;amount&#62;
    </td>
    <td>
      Sets the speed multiplier to the given amount. "/speedboost 1.5" will set the speed multiplier to 1.5x the normal speed.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      startcelebration
    </td>
    <td>
      /startcelebration &#60;id&#62;
    </td>
    <td>
      Starts a celebration effect on your character.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      stopeffect
    </td>
    <td>
      /stopeffect &#60;effect id&#62;
    </td>
    <td>
      Stops the given effect.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      teleport
    </td>
    <td>
      /teleport &#60;x&#62; (y) &#60;z&#62;
    </td>
    <td>
      Teleports you. If no Y is given, you are teleported to the height of the terrain or physics object at (x, z)
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      toggle
    </td>
    <td>
      /toggle &#60;ui state&#62;
    </td>
    <td>
      Toggles UI state.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      tpall
    </td>
    <td>
      /tpall
    </td>
    <td>
      Teleports all characters to your current position.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      triggerspawner
    </td>
    <td>
      /triggerspawner &#60;spawner name&#62;
    </td>
    <td>
      Triggers spawner by name.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      unlock-emote
    </td>
    <td>
      /unlock-emote &#60;emote id&#62;
    </td>
    <td>
      Unlocks for your character the emote of the given id.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      crash
    </td>
    <td>
      /crash
    </td>
    <td>
      Crashes the server.
    </td>
    <td>
      9
    </td>
  </tr>
</tbody>
</table>

## Game Master Levels
There are 9 Game master levels
<table>
<thead>
  <th>Level</th>
  <th>Variable Name</th>
  <th>Description</th>
</thead>
<tbody>
  <tr>
    <td>0</td>
    <td>GAME_MASTER_LEVEL_CIVILIAN</td>
    <td>Normal player</td>
  </tr>
  <tr>
    <td>1</td>
    <td>GAME_MASTER_LEVEL_FORUM_MODERATOR</td>
    <td>Forum moderator. No permissions on live servers.</td>
  </tr>
  <tr>
    <td>2</td>
    <td>GAME_MASTER_LEVEL_JUNIOR_MODERATOR</td>
    <td>Can kick/mute and pull chat logs</td>
  </tr>
  <tr>
    <td>3</td>
    <td>GAME_MASTER_LEVEL_MODERATOR</td>
    <td>Can return lost items</td>
  </tr>
  <tr>
    <td>4</td>
    <td>GAME_MASTER_LEVEL_SENIOR_MODERATOR</td>
    <td>Can ban</td>
  </tr>
  <tr>
    <td>5</td>
    <td>GAME_MASTER_LEVEL_LEAD_MODERATOR</td>
    <td>Can approve properties</td>
  </tr>
  <tr>
    <td>6</td>
    <td>GAME_MASTER_LEVEL_JUNIOR_DEVELOPER</td>
    <td>Junior developer & future content team. Civilan on live.</td>
  </tr>
  <tr>
    <td>7</td>
    <td>GAME_MASTER_LEVEL_INACTIVE_DEVELOPER</td>
    <td>Inactive developer, limited permissions.</td>
  </tr>
  <tr>
    <td>8</td>
    <td>GAME_MASTER_LEVEL_DEVELOPER</td>
    <td>Active developer, full permissions on live.</td>
  </tr>
  <tr>
    <td>9</td>
    <td>GAME_MASTER_LEVEL_OPERATOR</td>
    <td>Can shutdown server for restarts & updates.</td>
  </tr>
</tbody>
</table>

