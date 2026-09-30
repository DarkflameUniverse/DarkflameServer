# Web dashboard

DarkflameServer ships with a web dashboard for managing accounts, moderating, and watching the server. It replaces
the separate NexusDashboard. Master starts it when `enable_dashboard=1` is set in `masterconfig.ini`, and it listens on
the `port` in `dashboardconfig.ini` (2006 by default). It also talks to master over UDP on `net_port` and the port after
it (2010 and 2011 by default); keep those clear of the other servers' ports. Those UDP ports listen on `bind_ip` from
`sharedconfig.ini` like every other server; the web page itself listens on `listen_ip`.

This page is for server operators. Everything below the first section is optional.

## Getting started

1. Set `enable_dashboard=1` in `masterconfig.ini` and start the server as usual.
2. Create an operator account from the command line: `./MasterServer -a`. Accounts created this way get GM level 9.
3. Open `http://127.0.0.1:2006` on the server itself and sign in.

The dashboard only listens on `127.0.0.1` out of the box (`listen_ip` in `dashboardconfig.ini`). To reach it from
other machines, put it behind HTTPS with a reverse proxy (Caddy, nginx, ...) on the same machine, or set
`listen_ip=0.0.0.0` if you know what you are doing. Behind a proxy, set in `dashboardconfig.ini`:

```ini
secure_cookies=1
behind_proxy=1
dashboard_url=https://dashboard.example.com
```

`dashboard_url` is used for links in emails and webhook alerts.

### Tables

Every table remembers how you last sorted it and how many rows it shows per page. This is kept in your browser only
(per dashboard user), so it doesn't follow you to another browser; clearing site data resets it. Searches and the page
you were on aren't kept.

Detail pages (accounts, characters, properties and their 3D view, bug reports, play keys) show breadcrumbs for the way
you actually got there in this tab, for example Accounts > an account > a character > a property, so you can step
back to where you came from. Opened directly, a page shows its usual parent (Properties > a property).

## Files to back up

Next to the server binaries, the dashboard creates:

- `dashboard_jwt_secret`: signs sign-in sessions. Losing it only signs everyone out.
- `dashboard_totp_key`: encrypts two-factor login secrets. **Back this up with your database.** Without it, every
  account with two-factor login is locked out and has to be reset by staff with `accounts_manage` (GM 8+). You can
  also set `totp_key` (64 hex characters) in `dashboardconfig.ini` instead.
- `dashboard_oauth2_token.json`: only if you connect a mail account with OAuth2.

The [Backups](#backups) page copies the database itself (including settings changed on the Settings page), and lists
the files a restore also needs: these, your `*.ini` files and `vanity/`. Keep copies of them somewhere safe.

## Access levels and permissions

What people can do depends on their account's GM level. Out of the box:

| Level | Can |
|---|---|
| 0 | See their own account, characters, properties, strikes and trade/mail history; the leaderboards and the property showcase; use the API; change their password, email and two-factor login |
| 1+ | See the account, character, property and bug report lists; read the activity and command logs |
| 2+ | Kick and mute; read and write the moderation history; read player reports |
| 3+ | Approve names; handle player reports and bug reports; give strikes; see linked accounts, mailboxes, character history and who is online; rescue, teleport and restrict characters; read the chat log; send mail; economy reports and the world map |
| 4+ | Ban and lock accounts; email password reset links |
| 5+ | Moderate pet names, guilds, properties and leaderboards; revoke strikes; change a character's missions; change the chat filter's words; send and schedule announcements |
| 8+ | Manage accounts (create, change email or password, reset two-factor login) and GM levels; edit characters and replace their XML; give items back; read whispers, team and guild chat; send chat into the game; attach items to mail, to one player or everyone; import models; shut down worlds; schedule restarts and events; run economy checks; play keys, client files and the vanity files; see scheduled tasks, the audit log, server logs, crash dumps, server health and instance load; the developer tools |
| 9 | Delete accounts; grant permissions to single accounts and characters; change scheduled tasks, instance limits, settings and permissions (these two are always GM 9 only), webhooks and email settings; backups |

Each of these is a named permission, and you can change the lowest GM level allowed for any of them without rebuilding
or restarting:

- On the **Permissions** page (GM 9): click the lowest level that should have it. Changes apply straight away. The page has
  tabs for the dashboard permissions, the in-game commands and the config file names, with a search, a category filter
  and a *Changed only* switch.
- In `dashboardconfig.ini`: `permission_<name>=<level>`, for example `permission_accounts_ban=3`. The names are on the
  Permissions page.
- As an environment variable: `PERMISSION_ACCOUNTS_BAN=3`.

A level set on the page beats the file and environment; **Reset** on the page goes back to them. Staff permissions go
from 1 to 9, so players (GM 0) never get them, and GM 9 can always do everything (`permissions_manage`
and `settings` are always GM 9). What players do (their own characters, properties, strikes and trade history, the
leaderboards, the property showcase, and API access) can be set anywhere from 0 to 9: raise one to take it away from players, for example
`permission_api_access=1` so only staff can use the API. Account basics (password, email, two-factor login, signing
out) always work. Changing GM levels is also limited by rank: nobody but GM 9 can give or change a level at or above
their own.

Sending mail needs `mail_send` (GM 3+), but attaching items needs `mail_items` (GM 8), since it hands out items like
`/gmadditem`; mailing items to everyone needs `mail_broadcast_items` as well. Items go only to accounts you may manage;
mailing them to your own characters also needs `self_items` (below), and without it mail to everyone leaves them out.

Set `min_dashboard_gm_level` to keep lower levels out of the dashboard entirely.

Single accounts and characters can also be given or denied a permission or command with a grant (see
[Permission grants](#permission-grants)).

### Acting on yourself and on your own rank

GM 9 can use every tool on anyone, including their own account and other GM 9 accounts. Two safety rails remain: the
last GM 9 account that can still sign in (not banned or locked) can't be demoted, banned, locked or deleted, so make
another GM 9 first; and deleting your own account asks you to type your username. Below GM 9, staff can never act on an
account with a higher GM level than their own, or raise their own GM level. What else they may do is set on the
Permissions page:

- `manage_equal_rank` (default GM 9): use tools on other accounts with the same GM level.
- `self_tools` (default GM 1): tools that gain nothing, used on your own account and characters: rescue or move your
  characters, kick yourself, sign yourself out everywhere, email yourself a reset link.
- `self_items` (default GM 9): give your own characters something: edit coins, U-score, level and items, restore
  versions, replace XML, change missions, give lost items back, and mail items to your own characters. Without it,
  items mailed to everyone skip your own characters.
- `self_moderation` (default GM 9): change your own record or account security: ban, mute or lock (and lift them),
  strikes and warnings, history entries, restrictions on your characters, lowering your own GM level, changing your
  email, password or two-factor login without the current one, and deleting your own account.

Each of these works only together with the tool's own permission. Anything staff do to their own account or characters
is marked "(on their own account)" in the audit log and in alerts.

The same rules apply to the in-game slash commands that act on another player (see below).

### In-game slash commands

The Permissions page also lists every in-game slash command with the GM level it needs. The list comes from the world
servers: each one stores the commands it registered in the `slash_commands` table when it starts, so the page is empty
until a world has started once.

**Commands that do the same thing as a dashboard permission use that permission's level.** Change it on the
permission's row and the game follows at once; the command's card says which permission it follows, and the
permission's row lists its commands ("In game too"). The pairs:

| Dashboard permission | Commands |
|---|---|
| `accounts_kick` | `/kick` |
| `accounts_mute` | `/mute` |
| `accounts_ban` | `/ban` |
| `server_announce` | `/announce`, `/setanntitle`, `/setannmsg` |
| `mail_items` | `/mailitem` |
| `moderate_properties` | `/approveproperty` |
| `players_view` | `/showall`, `/findplayer`, `/spectate` |
| `health_view` | `/uptime`, `/metrics` |
| `worlds_manage` | `/shutdown` |
| `server_restart` | `/shutdownuniverse` |
| `server_live_update` | `/liveupdate` |

Commands that only act on your own character (`/gmadditem`, `/givemoney`, `/setcurrency`, `/giveuscore`, `/setlevel`)
and `/teleport` aren't paired: they keep their own level. The pairs are declared on the commands (`dashboardPermission`
in `SlashCommandHandler.cpp`).

The world servers read the permission levels the dashboard stored (its page, `dashboardconfig.ini` and its
environment), so set `permission_<name>` for the dashboard only.

A `command_level_<name>` value for a paired command (from an older setup, a config file or the environment) still wins:
the card marks it "overrides accounts_kick", and **Drop the override** makes the command follow the permission again.
For a value in a file or the environment, the page stores `command_level_<name>=permission` over it, which you can also
put in the file yourself.

Upgrading changes no levels by itself: the first time a world of an existing server starts with this version, each
command that now follows a permission but needed a different level before (for example `/mute`, GM 6 in the code while
`accounts_mute` is GM 2) keeps that level as an override. The page marks it "kept from before pairing", the audit log
records it as `change_command_level` by `[upgrade]`, and **Drop the override** makes the command follow the permission
from then on. A new server has nothing to keep, so its commands follow the permissions from the start.

Other commands have their own level, `command_level_<name>`, where the name is the first alias in lowercase with
anything other than letters and digits turned into `_` (e.g. `command_level_spawn`, `command_level_leave_zone`):

- On the page: stored for `worldconfig.ini` and beats the file and environment. Running worlds reload their settings at
  once; no restart.
- In `worldconfig.ini` or `sharedconfig.ini`: `command_level_spawn=6`, picked up when the worlds reload their settings
  (`/reloadconfig`) or restart.
- As an environment variable of the world servers: `COMMAND_LEVEL_SPAWN=6`.

Changes need `permissions_manage` and go in the audit log (`change_command_level`, and `change_permission` lists the
commands that followed).

Some limits come from the code and can't be changed, for paired commands too:

- Staff commands (anything above GM 0 in the code) go from 1 to 9, so players never get them. Player commands can be
  raised to any level, e.g. to stop players using `/pvp`.
- `/execute` never goes below GM 8: it runs another command as another player, with that player's GM level.
- `/setgmlevel` always stays at GM 0, so staff who lowered their own level can raise it again (it never goes above the
  account's GM level).
- Commands the game client acts on by itself (emotes, team, friend and ignore commands) have fixed levels; the page
  hides them unless you tick **Commands the client handles**.

A value outside those limits is ignored. Commands used at a level above GM 0 are written to the command log.

**Commands that act on another player follow the dashboard's rules for its tools.** GM 9 may use them on anyone. Below
GM 9, never on an account with a higher GM level (while someone plays at a lower level with `/setgmlevel`, their
account's level still counts), on your own level only with `manage_equal_rank`, and on yourself only with the matching
permission:

- `self_tools`: `/kick`, `/kill`
- `self_items`: `/mailitem`
- `self_moderation`: `/ban`, `/mute`. `/ban` also refuses the last GM 9 account that can sign in.
- Other players only (on yourself they work as before): `/teleport <player> <player>`, `/setlevel <level> <player>`,
  `/execute as <player>`, and `/tpall`, which leaves players you may not move where they are.

A refused command says why in the chat and names the permission. The page shows each command's rule on its card.

### Permission grants

A grant gives one account or character a permission or command on top of what its GM level allows; a deny takes one
away. What someone may do:

    allowed = (GM level allows it OR a grant covers it) AND no deny covers it

A grant or deny names one of:

- a dashboard permission (`permission`); it also covers the in-game commands that follow it
- an in-game command (`command`)
- every permission of a category (`permission_group`), for example Accounts
- every command up to a GM level (`command_group`), 1 to 9

Each has an optional expiry, a note, and who gave it and when. Rules:

- A deny beats a grant. Denies never apply to GM 9 accounts.
- `settings` and `permissions_manage` stay GM 9 only: they can't be granted and no group covers them. Commands with a
  fixed level and commands with a floor above GM 1 (`/execute`) can't be granted.
- On the dashboard only the account's grants count. In game the account's and the logged-in character's count.
- `min_dashboard_gm_level` still keeps lower levels out of the dashboard entirely.
- Grants count wherever permissions are checked: pages and API routes, the rank rules (`self_*`, `manage_equal_rank`),
  API access and API key scopes (a key never does more than its owner may now), and live updates over the WebSocket.

**Managing them** needs `grants_manage` (default GM 9). Nobody grants or denies what they don't hold themselves (every
permission of a group; a command they may use; command groups only up to their own GM level), and only on accounts the
rank rules let them manage (their own with `self_moderation`). Removing a grant follows the same rules. Where:

- The **Grants** tab of the Permissions page: every grant in force, the history, and a form that searches accounts or
  characters by name. With only `grants_manage` the page shows just this tab.
- The **Permission grants** card on account and character pages. Players see their own grants there, read-only.

The form offers only what you may grant. Changes apply at once: the dashboard reads the account's grants with every
request, and online players get them through master (`REFRESH_ACCOUNT` / `REFRESH_CHARACTER`) without relogging. Every
change goes in the audit log (`grant_permission`, `deny_permission`, `remove_grant`). Removed grants are kept, so the
list is also the history.

API: `GET /api/grants/catalog`, `GET /api/grants` (`?account=ID` or `?character=ID`), `POST /api/grants`
(`{targetType, target, kind, name, deny, expiresAt, note}`), `POST /api/grants/:id/remove`. Stored in the
`permission_grants` table.

## Settings

The **Settings** page (GM 9) lists every setting the servers read, grouped by what it's for (Server, Gameplay,
Players and access, Email, Dashboard, Data retention) rather than by .ini file; each setting still shows its name and
where its value comes from (an .ini file, an environment variable, this page, or the built-in default). That includes
settings the shipped .ini files leave out (such as `closed_to_non_devs`), with the default the server uses when nothing
sets them.

- Inputs match the setting: switches for on/off (saved as `1`/`0`), number boxes with their range and unit, dropdowns
  for fixed choices, and ID lists that show the zone, item or reward code names.
- Settings that only matter when another one is on stay out of the way until it is: the MySQL settings appear when
  the database is `mysql`, hardcore mode's settings when it's switched on, OAuth2's when email sign-in is `oauth2`.
- Edits aren't sent until you press **Save changes** (or Ctrl+S). They're saved together: if one is invalid, nothing
  is saved and that setting is marked. **Undo** puts one back; **Remove the value set here** goes back to the file's
  value or the default.
- A value set here is used when the files and environment leave the setting out. If a file or the environment sets
  it, keep **Override the value from the file** ticked so your value wins.
- Saving tells every server to reload its settings straight away. Settings marked *restart* are only read when a
  server starts; the page says which ones need it after saving. Settings marked *unused* are in older .ini files but
  not read by this version (hidden unless **Show unused** is on).
- An environment variable named after a setting in upper case (for example `MAX_CLIENTS`) still overrides the files.

Order of priority, highest first: a value set here with override, environment variable, the server's own `.ini`,
`sharedconfig.ini`, a value set here, the built-in default.

The database connection (`database_type`, `sqlite_*`, `mysql_*`), `jwt_secret` and `totp_key` are shown read-only with
a lock: the servers need them before they can read anything from the database, so change them in the .ini file or the
environment variable the page names, then restart. The same goes for the programs and folders the servers run or read
files from: `backup_mysqldump`, `backup_folder`, `client_location` and `dump_folder` can only be set in the .ini files
or the environment, so the `settings` permission can't be turned into access to the machine. Values set for them on
the Settings page by earlier versions are ignored. Programs such as `mysqldump` are run directly, without a shell.
Secrets (passwords, tokens, keys) from files are never copied into the database; secrets set on the web are stored in
the database and can't be read back from the page.

Where the rows come from and when they go: every server reports the keys of the files it read when it starts
(`ConfigSync::Sync`, table `server_config`). A key taken out of a file is forgotten the next time a server that reads
that file starts: its row is deleted when it came from the file (not an environment variable), has no value set on this
page, and isn't a permission level. Rows with a value set here stay until that value is removed.

The shipped `resources/*.ini` files list every setting in the catalog (`dDashboardServer/routes/SettingsCatalog.cpp`)
as `key=default` under a comment with its title and description, or name it in a comment (numbered families such as
`event_1`...`event_8` and the icon framing keys); the test `SettingsCatalogTests.ShippedFilesListEveryCatalogSetting`
fails when one is missing, and running it with `DLU_WRITE_INI_TEMPLATES=1` adds the missing ones to the files under
their section. CMake copies a file that isn't in the build folder yet and appends missing keys (without comments) to
one that is; values already in a server's files are never changed by it.

### Setting history

The **History** tab of the Settings page (same `settings` permission) lists every change made on the Settings page, and
by [scheduled events](#scheduled-events): the value set on the web before and after (and the file's value, which
applies when there is none), who changed it and when. A setting changed here has a **History** link on the Settings
page for just that setting. **Undo** puts the earlier value back through the same save as the Settings page, so it is
checked, audited and recorded as a new change; it is refused if the setting was changed again since (undo the newer
change first). Secrets are recorded without their values and can't be undone. Changes to permissions and command levels
are in the audit log, not here.

## Signing in and accounts

### Two-factor login

Anyone can turn on two-factor login on their account page with an authenticator app (Google Authenticator, Authy,
1Password, Bitwarden, ...). They get 10 single-use recovery codes for a lost phone.

To require it for staff, set `require_2fa_gm_level`, for example `3` for moderators and up. Staff at that level who
haven't set it up can only reach their own account page until they do.

If someone loses their phone and their recovery codes, staff with `accounts_manage` (GM 8+) can use **Reset 2FA** on
their account page.

### Forgotten passwords

With [email](#email) set up, **Forgot your password?** (`/forgot_password`) sends a reset link.

Players with two-factor login can also choose a new password there without email: their username, a current code from
their authenticator app, and one of their recovery codes. Both codes are needed, so someone who found the printed
recovery codes, or has the phone, can't take the account alone. The recovery code is used up, every dashboard session
and API key of the account is signed out, and an emailed reset link that's still open stops working. The game
password changes too.

It's limited like signing in: 10 tries per address every 15 minutes, and wrong codes count as
[failed sign-ins](#failed-sign-ins-locking-and-deleting-accounts). The page gives the same answer for unknown usernames, accounts without two-factor login and wrong codes, so it
can't be used to find accounts. Each reset goes in the audit log (`password_reset`), to the `security` webhook event,
and by email to the account's address if it has one. Banned and locked accounts can't use it. Turn it off with
`password_reset_recovery_codes=0` (Settings, Players and access); with neither email nor this, the page is gone.

### Email

Email is used for password resets, address confirmation and [report emails](#saved-views-and-report-emails). Set
`smtp_host`, `smtp_from_address` and `dashboard_url`, plus either `smtp_username`/`smtp_password` or OAuth2
(`smtp_auth=oauth2`, needed for Gmail and Microsoft 365). The comments in `dashboardconfig.ini` list every option.
Use **Send Test** on your account page to check it (`email_settings`, GM 9).

### Registration

Off by default. With `allow_registration=1` (Settings, Players and access) anyone can make an account at `/register`.
`registration_requires_play_key` (on) asks for a play key, and `registration_requires_email` (off) for an email
address, when email is set up.

Staff with `accounts_manage` can also create accounts on the Accounts page. While the auth server uses play keys, a
GM 0 account can only log in to the game with one, so give it a key there (create one on the Play keys page first).

### Failed sign-ins, locking and deleting accounts

Wrong passwords, wrong two-factor codes, failed password recovery attempts and wrong passwords when downloading a
backup all count as failed sign-ins. After 5 of them within 15 minutes from one address, that address can't try that
account for 15 minutes; other addresses, including the owner's, still can. As a backstop against guessing from many
addresses, 25 failures from anywhere without a successful sign-in in between lock the account for 10 minutes.
Unlocking the account on its page ends both.

**Lock** on an account page (GM 4+, `accounts_ban`) sets the account's Locked flag: it can't log in to the game or the
dashboard until it is unlocked, it is kicked from the game, and its open dashboard pages are closed. Locks are in the
moderation history and the audit log.

Open dashboard pages (their live-update connections) are checked again every minute and whenever their account
changes, so a ban, a lock, a lower GM level or **Sign out everywhere** reaches them straight away instead of when the
session runs out. Results of actions that finish later (a kick, an email sent, ...) go only to the account that
started the action, and only it can ask for them.

**Delete** (GM 9, `accounts_delete`) removes the account's characters with everything tied to them (properties and
their models, pets, mail, friends, snapshots, ...) and the rows about the account itself (tokens, recovery codes,
moderation history, strikes, login addresses, preferences). It is all or nothing. The audit log, chat log and player
reports are kept as the record of what happened.

## Live updates

Pages update on their own: world servers tell the dashboard (through master) as soon as they write something it shows,
and it pushes that to open browsers. Pages about one thing (a character, an account) update in place when it changes:
only the parts the server now shows differently change, and what you're typing stays. When the page can't be patched
that way (its layout changed) and you're typing, it offers a refresh instead.

Moving between pages doesn't reload the dashboard: the next page is fetched and swapped in, and the menu, the top bar
and the live connection stay. A thin bar at the top shows while it loads; if it can't be fetched, the page says so with
**Try again** and **Open it normally**. Back, forward, reloading, links opened in a new tab and links with a `#` filter
work as before. Pages with a 3D view (World 3D, a property and its 3D view, the UGC server page) and pages reached from
them load normally. Leaving a page with unsaved changes (Settings, Vanity) asks first.

For page scripts (`static/js/nav.js`): listeners a page adds to `document` or `window`, its `setInterval` timers, its
`Live` watchers and its DataTables are removed when another page is swapped in; its scripts run again when it's opened
again, and `DOMContentLoaded`/`load` handlers they add run once they have all run. Elements a page appends to `<body>`
are removed unless marked `data-nav-keep`. `Nav.go(url)` opens a page, `Nav.refresh()` updates the current one in place;
`goTo(url)` and `reloadInPlace()` (`common.js`) do the same, or load normally without `nav.js`. `document` gets
`dash:page` after a page is swapped in, `dash:leave` before, and `dash:refreshed` after an in-place update (pages that
format server values in the browser, like times, do it again then). A page that must always load on its own adds `data-nav="reload"` to any element; a link with `data-nav="off"`
always loads normally.

View choices you make on the pages (show staff, filters, the 3D viewer's switches, ...) are saved to your account, so
they follow you to other browsers.

The menu's groups stay open or closed from page to page (kept in this browser): the group of the page you're on opens
and stays open until you close it. On wide screens the &#9776; button in the top bar hides the menu, and it stays hidden until you show it again.

On narrow screens the menu folds into a **Menu** button, and the moderation queues and online players show as cards
with their buttons, so you can approve names or kick someone from a phone.

## Running the server

### Worlds

The home page lists every running world with its zone, instance and clone ID, and for staff the property on a clone
and the world's address. **Shut down** (GM 8+, `worlds_manage`) closes one instance; its players are disconnected.
Accounts without `players_view` (players, by default) see no clone IDs: running property instances are merged into one
row per zone with how many properties are open and their players, so nobody can tell who is on which property. The
same goes for `/api/status`.

The **Server Status** card shows auth, chat and, while master starts it (`enable_ugc_server`), the UGC server. Staff
with `health_view` also get a **UGC Server** card: whether it is up and for how long, the models and cars and rockets
waiting, made and failed (from the database, the UGC server's work list), its busy workers and the space its files take
(from its last traffic report), with a link to the UGC page. `/api/servers/ugc` returns the same.

### Announcements and restarts

From the home page:

- **Announcement** (GM 5+, `server_announce`): a popup and chat message for everyone online.
- **Scheduled restart** (GM 8+, `server_restart`): players are warned in game at 60, 30, 15, 10, 5, 2 and 1 minutes
  and 30 and 10 seconds before, then the whole server shuts down cleanly. **It does not start itself again**: run it
  under a process supervisor (systemd with `Restart=always`, a Docker restart policy, ...) so it comes back. Who
  scheduled it (`GET /api/server/restart`) is also only for `server_restart`; everyone sees when and why.

### Scheduled announcements

**Scheduled Announcements** (GM 5+, `announcements_schedule`) repeats an announcement on a schedule: a title and message,
every world or only chosen zones, a schedule in the same syntax as [scheduled tasks](#scheduled-tasks) (cron or
`@every 2h`, in UTC, at most once a minute), and optional start and end dates. The dialog shows the next sends as you
type. **Send now** shows it straight away; the schedule carries on. A send that falls while the dashboard or the master
server is down is skipped, not sent late. Creating, changing, sending and deleting are audited. For announcements that
belong with something else (said when an event starts, repeated while it is on), add an announcement part to a
[scheduled event](#scheduled-events) instead; the sends of both show on its calendar.

### Scheduled events

**Scheduled Events** (under Live Ops) switches things on together and off again later. An event has **parts**, any mix
of:

| Part | Needs | While the event is on |
|---|---|---|
| **Game feature** | `events_manage` | the feature is in an event setting (see below) |
| **Vanity changes** | `vanity_manage` | vanity files are switched on or off, an overlay file is laid over the others and NPCs are taken out (see [Vanity](#vanity)) |
| **Live event** | `live_events_manage` | a [live event](#live-events) runs: it starts when the event does and ends when it ends (a live event runs at most 7 days; one cut short that way isn't started again) |
| **Announcement** | `announcements_schedule` | a message when it starts, repeated on a schedule while it is on (cron or `@every`, as for [scheduled announcements](#scheduled-announcements)), and another when it ends; each is optional |
| **Restart** | `server_restart` | a [scheduled restart](#announcements-and-restarts) when it starts or when it ends, with the warning time and reason you give (one already scheduled is left alone) |

Adding, changing or removing a part needs the permission its own page needs; changing an event's name, times or mode,
cancelling or deleting it needs the permissions of every part it has. Seeing the page needs any of them; parts you
can't change are shown read-only.

**When** it is on:

- **Once**, from a start to an end (at most a year apart), like the old events calendar;
- **By rules**, again and again: every October, full-moon nights, Friday evenings (the rules are below).

Its **mode** is *Off*, *Scheduled* (on when its times or rules say) or *Always On*. **Priority** decides which event
wins where events that are on change the same vanity NPCs or files: they are laid on from the lowest priority to the
highest (by id when equal), so the highest wins.

The dashboard checks every 5 seconds. When an event turns on it starts each of its parts, and when it turns off it ends
them. Each part remembers whether it was started, so every start and end happens once, even if the dashboard or the
servers restart in between; a part that couldn't start (every event setting taken, a live event the client no longer
has) says why and is tried again. Changing an event that is on keeps the parts you didn't change running; changed and
removed parts are undone (a feature's setting put back, a live event ended, the vanity NPCs respawned) and changed ones
started again. Announcements and restarts only act the moment their event starts or ends, so changing one while the
event is on doesn't say it again, and deleting an event undoes its parts without its end announcement or restart.
**Cancel** (for an event that is on once) switches it off now and ends its parts as if it had ended. Everything is
audited, and *Scheduled event started/ended*, *Event waiting for a slot* and the parts' own alerts (*Live event
started*, *Restart scheduled*, ...) go to the `server` webhook event. An event that is on once and that the dashboard
was down for entirely is marked missed.

The page lists the events with what each part last did, when each is on next (in your browser's time) and a month
calendar that also shows, dashed, what is scheduled on its own pages: live events, the sends of repeating
announcements and a scheduled restart. **Export** copies an event as JSON and **Import** takes one or a list of them.

#### Game features and the event settings

The game has eight event settings, `event_1` to `event_8`: auth sends them to the client at login, and a world server
loads objects that are gated on a feature (`gatingOnFeature` in the zone's scene files) only when the feature is in one
of them or already unlocked for the client version (`version_major/current/minor`, the `FeatureGating` table).

Pick the feature from the list: every `gatingOnFeature` value in the client's scene files, with the zones that use it,
and every `FeatureGating` name (marked when it is already on at your client version, where it changes nothing in the
zones). When the part starts, the dashboard puts the feature in the first event setting nothing gives a value (a file,
the environment or the Settings page) as a `sharedconfig.ini` value that wins over the files; when it ends, the setting
is put back as it was, unless someone changed it by hand meanwhile. A feature that is already in a setting is left
there and isn't put in a second one. Both go through the Settings save, so they're in the audit log and the
[setting history](#setting-history). If every setting is taken the part waits, and says so. **Event settings now**
shows what each setting holds and which event put it there.

What takes effect when: new logins get the change straight away. Objects are only loaded when a zone starts, so worlds
of the zones that use the feature and are already running keep what they had until they restart; the event lists them
with their players, and staff with `worlds_manage` can shut one down from there (a fresh instance starts when someone
goes there).

#### Rules

A schedule by rules is a list of rules, on when **any** of them matches or when **all** of them do:

| Rule | JSON | On |
|---|---|---|
| Every year | `{"type": "yearly", "from": "10-01", "to": "10-31"}` | those days every year, both included; `12-20` to `01-02` runs over the new year |
| Once | `{"type": "dates", "from": "2026-12-20", "to": "2027-01-02"}` | between two dates (a date alone is the whole day) or times (`2026-10-31T18:00`, the end not included) |
| Days of the week | `{"type": "weekdays", "days": ["fri", "sat"]}` | those days |
| Times of day | `{"type": "time_of_day", "from": "18:00", "to": "02:00"}` | every day between those times; may run past midnight |
| Moon phase | `{"type": "moon", "phase": "full_moon", "days": 0}` | the day the phase falls on (and `days` either side); or with `"hours": 12` instead, that many hours either side of the exact time. Phases: `new_moon`, `first_quarter`, `full_moon`, `last_quarter` |
| Group | `{"type": "group", "match": "all", "rules": [...]}` | rules inside rules (up to 4 deep) |

Any rule can have `"not": true`. Moon phases are worked out with the method from Meeus' *Astronomical Algorithms*
(to a minute or two), so no dates need entering. Days and times in the rules are in the schedule's time zone
(`utcOffset`, in minutes from UTC; a new schedule starts in your browser's). It is a fixed offset: it doesn't follow
daylight saving. Mind it for whole-day rules: a full-moon *day* in UTC is 8 PM to 8 PM in New York, so pick your own
offset if the day should be yours. The editor builds the rules (or edit the JSON directly), says which time zone they
are in, and lists when the schedule will be on next, in your browser's time.

#### Examples

Halloween every October: `halloween.xml` switched on and `summer.xml` off, the game's Halloween content (a feature;
use one your client has), fireworks in Nimbus Station and a greeting when it starts. And a werewolf on full-moon nights
that wins over Halloween where they meet:

```json
[
	{"name": "Halloween", "mode": 1, "priority": 0,
	 "schedule": {"utcOffset": 0, "match": "any", "rules": [{"type": "yearly", "from": "10-01", "to": "10-31"}]},
	 "parts": [
		{"kind": "vanity", "config": {"fileSwitches": {"halloween.xml": true, "summer.xml": false}, "file": "", "removals": []}},
		{"kind": "feature", "config": {"feature": "oct2011content"}},
		{"kind": "live_event", "config": {"type": "celebration", "title": "Halloween fireworks", "message": "", "zones": [1200], "instance": -1,
			"config": {"effectId": 4, "effectType": "fireworks", "interval": 30}}},
		{"kind": "announcement", "config": {"title": "Halloween", "message": "Spooky season has begun!", "zones": [], "atStart": true, "repeat": "", "endMessage": ""}}]},
	{"name": "Full moon", "mode": 1, "priority": 10,
	 "schedule": {"utcOffset": 0, "match": "all", "rules": [{"type": "moon", "phase": "full_moon"}, {"type": "time_of_day", "from": "20:00", "to": "24:00"}]},
	 "parts": [{"kind": "vanity", "config": {"file": "full-moon.xml", "removals": ["Friendly Farmer"], "fileSwitches": {}}}]}
]
```

An event that is on once has `"schedule": null` and `"startsAt"`/`"endsAt"` (unix seconds) instead.

### Live events

**Live Events** (GM 8+, `live_events_manage`) starts something in game now, for 1 minute to 7 days, in the zones you pick
(and optionally one instance of them). It is announced in those zones when it starts and when it ends (with a summary
and the top players), audited, and sent to the `server` webhook event. Every running world of those zones runs it in
each of its instances, worlds that open before it ends join in, and everything it spawned is removed when it ends, is
ended early (**End now**), or the world shuts down. The page shows each instance's progress live and the top players.

- **Treasure hunt**: hides up to 50 objects (picked from the client's objects) per instance, spread out on ground the
  zone itself uses (where its NPCs walk and its enemies spawn, snapped to the navmesh when there is one). A player finds
  one by walking up to it (or smashing it), for coins, an item and an effect you choose.
- **Bonus**: multiplies coins from loot, U-score from missions and achievements, and loot drop chances (1 to 10). With
  no zones it applies everywhere. Bonuses running at once don't stack; the biggest counts.
- **Invasion**: waves of enemies (up to 5 kinds from the client's enemies) around the zone's spawn point or a player,
  with a counter of who smashed how many. At most 60 are alive per instance.
- **Celebration**: plays an effect from the client's BehaviorEffect table on every player every few seconds.

Players see what is running in their world with `/challenge`. To run one at set times (every Friday evening, with
Halloween), add a live event part to a [scheduled event](#scheduled-events); it shows here while it runs.

Treasure hunts and invasions announce their end in each world with that world's count. The dashboard announces
the end of other kinds of events.

### Community challenges

**Challenges** (everyone with `challenges_view`, GM 0; changing them needs `challenges_manage`, GM 8) are goals the
whole server works on, such as "smash 10,000 enemies this week": a player statistic or a map event kind (optionally only
one object, only some zones, and staff left out unless ticked), a target, a start and an end. World servers count each
character's part where the game records it (what the client reports on its own doesn't count). The dashboard announces
it in game when it passes each of `challenge_milestones` (25, 50, 75 by default) and when it's complete.

When the target is reached, every character that added at least the reward minimum gets the rewards: items by mail (one
mail per kind), coins in game (online players at once, others the next time they type `/challenge`). Nobody is rewarded
twice. Creating, changing, cancelling, completing and rewarding are audited.

In game, `/challenge` shows the running challenges, how far they are and what you added, and gives any coins waiting.
With the public status page on, public challenges (running, or finished in the last week) and running live events show
there too (`public_status_challenges`, `public_status_events`).

The complete and over announcements are sent once the dashboard can reach the worlds: a challenge that ends while the
dashboard is restarting is announced when it comes back, up to an hour after it ended. Milestones work the same way.

### Scheduled tasks

The **Scheduled Tasks** page (GM 8+ to view, GM 9 to change) lists the jobs the dashboard runs on its own:

| Task | Default schedule (UTC) | Does |
|---|---|---|
| `economy_checks` | `15 0 * * *` | Anomaly checks for the day before |
| `economy_compaction` | `30 0 * * *` | Merges old ledger rows into months, deletes old trades and mail |
| `log_pruning` | `45 0 * * *` | Deletes old log rows, chat, login addresses, health samples, player positions and task runs |
| `message_capture_pruning` | `50 0 * * *` | Deletes saved message inspector captures past `inspector_session_days` or over `inspector_max_mb` |
| `character_snapshots` | `0 4 * * *` | Saves every character that changed since its last snapshot |
| `database_backup` | `30 3 * * *`, **off** | Copies the database (see [Backups](#backups)) |
| `report_emails` | `0 7 * * *` | Emails saved Economy views (weekly ones on Mondays) |
| `lift_expired_bans` | `5 * * * *` | Unbans accounts whose temporary ban has ended |
| `pet_names_auto_approve` | `0 * * * *` | Approves pending pet names that were approved for another pet before |
| `pet_owners` | `*/30 * * * *` | Looks up the owners of pets named before the game saved them |

For each task you can change the schedule, switch it off (it then only runs when someone presses **Run now**) and
run it straight away. Every run is kept with its log for `log_task_days` days; click a run to read it, or **Log**
to follow one that is still running.

Schedules are cron expressions in UTC (`minute hour day-of-month month day-of-week`), `@hourly`/`@daily`/`@weekly`/
`@monthly`, or a fixed interval like `@every 10m`. The edit dialog shows the next few run times as you type. If the
dashboard was down when a task was due, it runs once when the dashboard starts. The `economy_checks` setting from
earlier versions is replaced by the task's on/off switch.

### Backups

The **Backups** page (GM 9) makes a copy of the database and lists the copies on the server. SQLite databases are
copied with `VACUUM INTO` while the servers keep running; MySQL databases with `mysqldump` (set `backup_mysqldump` if it
is called something else, such as `mariadb-dump`). Copies go in `backup_folder` (default `backups` next to the server)
and only the newest `backup_keep` (default 7) are kept.

Scheduled backups are **off** until you switch the `database_backup` task on (daily at 03:30 UTC; change the time on
Scheduled Tasks). Downloading a backup asks for your password and two-factor code again, because it contains every
account's password hash and email address; each download is audited and sent to security webhooks. Wrong passwords
and codes there count as [failed sign-ins](#failed-sign-ins-locking-and-deleting-accounts). Backups are written with
owner-only permissions (0600).

Every new backup is checked before older ones are deleted, and one that fails its check is thrown away and the older
ones kept: SQLite copies get `PRAGMA integrity_check` and a row count of every table; MySQL dumps must be complete
(end with `-- Dump completed`). **Verify** checks a backup again: it only reads the file, runs in the background, and
is audited (`verify_backup`, `verify_backup_result`). For SQLite it also reports any two-factor secrets this server's
key can't decrypt.

A backup is only the database, including settings changed on the Settings page. The page lists what a restore also
needs; keep copies of these somewhere safe as well: `dashboard_totp_key` (or `totp_key` in `dashboardconfig.ini`),
your `*.ini` files, `vanity/` and `dashboard_oauth2_token.json`.

**Restoring SQLite:** stop all servers; copy the backup over the database file (`sqlite_database_path`); **delete
`<database>-wal` and `<database>-shm`** if they exist (a leftover `-wal` from the old database corrupts the restored
one); start the servers.

**Restoring MySQL:** stop all servers, then

```sh
mariadb -e "DROP DATABASE dlu; CREATE DATABASE dlu"
mariadb dlu < dlu-YYYYMMDD-HHMMSS.sql
```

(or `mysql`). Dumps from MariaDB 11+ begin with a sandbox-mode line that MySQL's client rejects; restore those with
`mariadb`, or delete the first line. Start the servers.

Restoring an older backup is fine: migrations bring it up to date when the servers start.

### Webhooks and alerts

On the **Webhooks** page (GM 9) add Discord, Slack or generic JSON webhooks and choose which events they get:
`bug_report`, `pending_name` (names waiting for approval), `moderation` (bans, locks, mutes, kicks and restrictions,
from the dashboard or in game), `security` (GM level and two-factor changes, API keys, recovery code use),
`economy_flag` and `server` (auth, chat or, while it is enabled, the UGC server going down or coming back, restarts,
events). JSON deliveries can be signed:
with a secret set, each request has `X-DLU-Signature: sha256=<hex HMAC-SHA256 of the body>`.

### Server health and instance load

**Server Health** (GM 8+, `health_view`) charts players online, running worlds, memory used by all server
processes, and whether auth, chat and the UGC server were up (the UGC server only while master starts it; grey where it
was off), over the last day, week or month (sampled once a minute, kept for `health_days`, 30). The **Servers** table
lists every server master knows (master, auth, chat, the dashboard, the UGC server, each world) with its state, how
long it has been up, and on Linux, when it runs on the dashboard's machine, its process ID, memory and CPU (of one
core, since the table last refreshed), plus what its last traffic report said: connections, ping, worker threads, and
for the UGC server its queue and storage. Server processes of this build that master doesn't list show as "Not listed".
`/api/servers` returns the table's data.

**Prometheus metrics** are off until `metrics_enabled` is switched on (Settings > Dashboard > Metrics). `/metrics` then
serves the Prometheus text format: players online (total, per zone and per instance), running worlds per zone, whether
master, auth, chat and the UGC server are up (`darkflame_ugc_enabled`, `darkflame_ugc_up`), memory and process count
per kind of server, the UGC server's work list (`darkflame_ugc_items`, labels `kind` model or modular and `state`
pending, done or failed), dashboard uptime and WebSocket clients, chat
messages by channel (and how many the filter blocked), today's coins, items and map events, moderation queues (names,
pet names, properties, bug reports, economy flags, player reports), active strikes, each scheduled task's last run, and
how long the database reads behind them took. Labels never contain account or character names or addresses. A scraper
sends `Authorization: Bearer` with either an API key of an account with the `metrics_view` permission (GM 8 by
default), or the shared `metrics_token` (at least 16 characters). `metrics_allowed_ips` can limit where requests come
from (addresses or IPv4 ranges like `10.0.0.0/8`), and the numbers are worked out at most every `metrics_cache_seconds`
(10), however often they are fetched. `/api/metrics` returns the same text for API keys (the key needs `metrics_view` in its scope). An example scrape config and
Grafana dashboard are in `docs/grafana/`.

**Instance Load** (`health_view`) records the players in each world instance at the same time and keeps them as long:
the busiest zones (most players in one instance, most instances and most players at once), and for a zone each
instance's players over time against its caps.

How many players an instance takes is decided by the master server: it sends a player to a running public instance of
the zone while it has fewer players than the zone's **soft cap** (joining a friend: the **hard cap**), and otherwise
starts a new instance; the hard cap is also the most connections that world server accepts. The caps come from the
client's `ZoneTable` (`population_soft_cap`, `population_hard_cap`). With `instances_manage` (GM 9) you can set other
caps per zone (up to 500), and up to 5 **spare instances**: the master keeps that many instances of the zone with room
running (checked every 10 seconds, one started at a time), so players moving in don't wait for a world to start. A
spare that stays empty still shuts itself down after 30 minutes like any empty world, and the master starts another.
A spare that stops before it has been up for 5 minutes (a crash, a missing file, a port in use) counts as a failure:
the next try waits 10 seconds, and each failure in a row doubles the wait, up to 30 minutes, so a broken zone isn't
restarted over and over. One that stays up for 5 minutes resets it; the master's log says how long it waits.
Changes reach the master straight away (no restart): new instances take the new caps, and running ones too, but a
running instance's hard cap can only go down, not above what its world server was started with. Private instances and
character selection aren't affected.


### Traffic diagnostics

**Diagnostics** (`health_view`, under Logs & Health in the sidebar) shows the load on every server: packets
and bytes in and out per second (all servers together, or one picked from the list or by clicking it), packets per
second of each server, HTTP requests per second of the dashboard and the UGC server with their errors, HTTP latency
(p50, p95 and p99 of the busiest web server), the busiest packet and game message types, and the HTTP routes with
their request counts, errors and latency. **Live** shows the last 5 minutes at one second and updates as reports arrive
(the `traffic` WebSocket topic); **1 hour** is at 10 seconds; **24 hours** (5 minutes) and **7 days** (30 minutes)
come from the database. The servers table also shows each server's RakNet connections, average ping and resent
messages, and the worker threads of the dashboard and the UGC server. While the UGC server is enabled, its card says
whether it is up (throttled, paused), how many items wait and failed (a link to the failed ones), its busy workers, CPU,
memory and stored files; what it made and how long each took is on the UGC page.

How it is counted:

- Every server (master, auth, chat, each world, the dashboard's master link, UGC) counts in `TrafficStats`
  (`dCommon/TrafficStats.h`): `dServer` counts each packet it receives (`Receive`, `ReceiveFromMaster`) and sends
  (`Send`, `SendToMaster`, `Disconnect`) into one-second buckets, keyed by service and packet ID, and for game messages
  the game message ID; replica construction and serialization are counted under `RAKNET`. A broadcast counts once per
  connection it goes to. Counting is on the main loop only and costs about 40 ns a packet. Each packet is also counted
  by peer: the server's own connections (players on auth and worlds; the worlds on chat and every server on master count
  as other servers), its master link, or other servers (a world's chat link, counted in `ChatServerLink`).
- The web server (`dWeb`) counts each request under its route pattern (`GET /api/players/:id`, so there is one entry
  per route; unknown paths are `(no route)`), its status class, the bytes of the body (a served file: its size) and a
  latency histogram. A deferred request counts when its answer goes out, so its latency includes the worker's time.
  Requests carrying `X-Darkflame-Server` (the dashboard's requests to the UGC server) count as another server's; the
  dashboard counts the requests it makes to the UGC server and the bytes they answer with. Each client address's
  requests and bytes are counted too, for the Network page's connection list.
- Every 5 seconds a server sends `SERVER_TRAFFIC` (`dNet/master/ServerTraffic.h`, about 600 bytes) to master with its
  seconds, its 24 busiest message types each way, its routes, RakNet's statistics for its connections (datagrams,
  resends, ping) and a few gauges (`http_deferred_pending`, `websocket_clients`, `workers_busy`, `workers_queued`,
  `workers_threads`; the UGC server adds `ugc_made_total`, `ugc_failed_total` and `ugc_evicted_total` since it started,
  `ugc_stored_bytes` and `ugc_max_storage_bytes`). Master passes them to the dashboard and sends its own there; the dashboard keeps its own.
  Newer servers add two optional sections at the end of the report, each after a marker byte: each second's packets by
  peer with its HTTP requests from and to other servers (about 20 bytes a second), and the 32 busiest remote ends
  (RakNet's datagrams, bytes, resends and ping for each connection, with the player's account and character on worlds;
  requests and bytes for each HTTP client address) with the rest summed. Reports without them (older servers) still
  read, and older readers stop before them.
- The dashboard keeps the last hour at one second in memory, the busiest message types per minute for an hour and per
  hour for a day, and writes one row per server and minute to `server_traffic` once a minute (in one batch, on the
  background thread), kept for `traffic_days` (30; Settings > Data retention, pruned by the Log pruning task). Latency
  percentiles are worked out from mergeable histograms (three buckets per doubling, from 0.1 ms), so a minute's are
  right; over 24 hours and 7 days a point shows the worst minute's.

### Network

**Network** (`health_view`, next to Diagnostics) draws the traffic live, from the same reports: game clients on the
left, auth, chat, the worlds (one box per zone; + shows each instance) and any other server that reports in the middle,
then master, then the dashboard and the UGC server, and web clients (browsers and API users) right of the dashboard. Servers appear as they report, so a new kind of
server shows up without changes. Each link has a lane each way, as thick as its bytes per second, with dashes moving
faster with more packets, and coloured by its load against its own peak over the last 5 minutes; hover it for the
numbers. Boxes show connections, average ping, resends, busy workers and live dashboard pages. Clicking a box shows its
links, its busiest message types each way over 5 minutes, its packets per second over 10 minutes and a link to
Diagnostics filtered to it (`/diagnostics?server=<key>`). It updates with the `traffic` WebSocket topic (every 2
seconds while reports arrive), stops drawing while the tab is hidden. **Diagram** or **List** (each box with its links, for phones) is remembered
per browser. Drag boxes to rearrange the diagram; links leave from the sides that face each other, and **Reset layout**
puts everything back.

The links, and how exact they are:

| Link | From | Exact? |
| --- | --- | --- |
| Game clients - auth, worlds | Each server's packets with its own connections | Yes (LU packets, not RakNet's acknowledgements and resends) |
| Server - master | Each server's packets with its master link | Yes |
| World - chat | Each world's chat link | Yes |
| Web clients - dashboard | The dashboard's HTTP requests less other servers' | Requests and answered bytes; request bytes aren't counted per second |
| Game clients - UGC server | The UGC server's HTTP requests less the dashboard's | As above; any browser fetching UGC files counts here too |
| Dashboard - UGC server | The requests the dashboard made and the bytes answered | Yes |
| Anything of a server that reports no split (older server) | Its totals, drawn dashed and marked estimated | No |

**Connections** lists every server's remote ends from its last report, grouped by address: game clients (RakNet
datagrams and bytes each way, ping, resends, and on worlds the logged-in account and character), web clients (requests
and bytes of the dashboard and the UGC server) and server links, with the rest of each server's connections summed. +
on the Game clients or Web clients box draws the busiest 8 of them in the diagram. IP addresses are personal data:
they are shown only with `network_ips` (Network addresses, level 9 by default; grant it like any other permission) and
are otherwise replaced by a token that stays the same for the same address until the dashboard restarts. They are kept
in memory from the last report only and never written to the database.

- `GET /api/diagnostics/network`: the live summary (what the `traffic` topic sends).
- `GET /api/diagnostics/network/server?key=world:1200:3`: one server's details.
- `GET /api/diagnostics/network/connections`: the connection list (addresses with `network_ips`).

**Prometheus**: `/metrics` has the same counters, totals since the dashboard started: `darkflame_net_packets_total`,
`darkflame_net_bytes_total` and `darkflame_net_datagrams_total` (labels `server`, `direction`),
`darkflame_net_messages_total` (`server`, `direction`, `service`, `message`), `darkflame_net_resends_total`, the gauges
`darkflame_net_connections` and `darkflame_net_ping_milliseconds`, `darkflame_http_requests_total` (`server`, `route`,
`status` class), `darkflame_http_response_bytes_total` and the histogram `darkflame_http_request_duration_seconds`
(buckets at every doubling from 0.1 ms), and the reported gauges as `darkflame_server_<name>`. `server` is `master`,
`auth`, `chat`, `dashboard`, `ugc` or `world:<zone>:<instance>`. Use `rate()` for per-second values.

### Logs and crash dumps

**System Log** (GM 8+, `logs_system`) shows the end of any server's log file (the newest by default; pick an older one
from the list), says which file you're looking at and when it was last written, and can download it. It also searches
the newest log files of one or all servers.

The UGC server's logs are `UgcServer_<time>.log`, listed with the others.

**Download logs** (on the System Log page) puts log files in one zip file. Pick a date range (a file is picked when
the time from its start, in its name, to its last write overlaps it), the servers (master, auth, chat, dashboard,
UGC, worlds), and for worlds any number of zones and a clone or instance. It can add crash dumps, keep only the lines
written in the date range rather than whole files, keep only lines with some text, and hide IP addresses (they become
`[ip]`). **Preview** lists the files and their total size first. The zip holds the files as they sit under `logs/`
(and `crash_dumps/`) plus a `manifest.txt` with the server version, who made it, the filters and each file. It is
built on a worker thread in the system's temporary folder and deleted once sent. `log_bundle_max_mb` (Settings, Data
retention; default 512, at most 4000) caps how much log text one download holds before compression; over it the
download fails with a message saying so. Every download is in the audit log (`download_logs`, with the filters, file
count and size).

The same works with an API key (`logs_system`):

```sh
# Preview: files, count, total_size, over_limit
curl -H "Authorization: Bearer $KEY" "https://dashboard.example/api/logs/bundle/preview?from=1790400000&to=1790500000&servers=world&zones=1200"
# The zip: only error lines of Nimbus Station worlds from that day, addresses hidden, crash dumps too
curl -OJ -H "Authorization: Bearer $KEY" "https://dashboard.example/api/logs/bundle?from=1790400000&to=1790500000&servers=world&zones=1200&trim=1&text=error&redact=1&crash=1"
```

Query values for both: `from`, `to` (Unix seconds; either may be left out), `servers` (comma separated `master`,
`auth`, `chat`, `dashboard`, `ugc`, `world`; default all), `zones` (comma separated zone IDs), `clone`, `instance`,
`crash=1`, `trim=1`, `text=`, `redact=1`. The download answers 404 when nothing matches and 413 when it would be over
`log_bundle_max_mb`.

Crash dumps: set `dump_folder` (for example `crash_dumps`, relative to the server binaries) and world servers and the
UGC server write a backtrace file there when they crash (the UGC server's is `Crash_UgcServer_<start time>_<pid>.log`; `generate_dump=1` also
writes a memory dump on Windows). Server Health lists them
to read or download for staff who also have `logs_system`.

The **Activity Log**, **Command Log** and **Audit Log** pages show zone changes, slash commands used and what staff did
on the dashboard. How long each is kept is under Settings, Data retention (`log_*_days`; 0 keeps everything).

## Players and characters

### Online players

**Online Players** (GM 3+, `players_view`) lists everyone in game: character, account, world and position, updated
live. From there you can kick a player, rescue them to another zone, or teleport them to another player in the same
world (`characters_rescue`). A rescue lands on the zone's usual spawn point, or on one you pick from the named spawn
points in the zone's scene files (the ones rocket launchers use; needs `client_location`). A character that isn't
online has its saved position changed instead.

### World 3D

**World 3D** (GM 3+, `players_view`) shows a zone in 3D the way the game draws it: its ground, the models of its
objects and its sky, with everyone online moving on it live. Pick the zone and, when it has several, the instance.
Click a player to see who it is, double-click (or **Follow**) to keep the camera on them; click an object for its name,
LOT and the scene it belongs to. Needs `client_location`.

The view fills the window (the page doesn't scroll; the wheel only zooms). View settings are in the **Layers** tab
(on phones, the **Panel** button opens it from the bottom) and are remembered per account.

- **Scenery** (on by default) draws every object in the zone's scene files with its model, from the client's .nif
  files, as the property view does; **Sky** switches the zone's sky. Objects the game doesn't draw (trigger and
  blocking volumes: blocking-volume types, `renderDisabled`/`CreateNULLRender`, and `carver_only` objects, which the
  client never loads) are left out; **Hidden objects** shows them see-through in pink for debugging. **Fog** (off by
  default) adds the zone's fog. Models are drawn with the game's shaders (see *Game shaders* under Properties in 3D). Models load nearest first around where the camera
  looks; **High / Medium / Low detail** sets how far they're drawn and how sharp (Medium by default). Objects drawn with
  a model get no marker; **Markers on objects with a model** (Layers tab) brings the markers back. They can be clicked
  either way.
- **Terrain** shows the zone's terrain file (.raw), read with the same parser as the world server's terrain code
  (`Raw`, dCommon), in one of its layers: **Textured** (its four textures blended as the game does), **Color map** (the
  tint painted over the ground), **Texture blend map** (the weights of the textures as colours) or **Scenes** (which
  scene each part of the ground belongs to, coloured with the game's scene colours, with a legend of the scenes' names
  from the .luz and their share of the ground in the Layers tab).
- **Flairs** (Layers tab, with Models on) are the grass, flowers and small rocks the terrain file strews over the
  ground (models from FlairTable), drawn only near the camera like the game does.
- **Objects** are markers coloured by what they are (enemies, vendors, mission givers, quickbuilds, launchers,
  collectibles, activities, smashables, other); the Layers tab switches each kind on and off.
- **Paths** (Layers tab, off by default) are the zone file's paths as lines, by type. They are most of a zone file, so
  they're only read when switched on; everything else needs just the start of the .luz (its scene list, spawn point and
  terrain file name).

**Replay** (GM 5+, `players_history`) plays back where players went over a time range (up to 7 days at a time) with a
scrubber, speeds up to 1800× and fading trails. The dashboard keeps a player's position every `position_history_seconds`
seconds (5) while they move and every 30 seconds while they stand still, for `position_history_days` days (3); switch it
off with `position_history=0` (Settings, Data retention, Player movement). Every replay opened goes in the audit log
(`view_position_history`).

**Heat map** (`reports_view`) plays the World Map's events of one kind (kills, drops, deaths, ...) day by day as coloured
squares on the ground: one day a frame, seven days a frame, or everything so far.

The view starts on the zone's objects and spawn point. Terrain that is only a flat plane far from everything (like
Venture Explorer's) is not drawn.

### Editing, history and lost items

Everything that changes a character (the editor, restoring an old version, uploading XML) first disconnects its
player if they're online, saves a snapshot of the character as it was, then writes the change and notes it in the
audit log. Nothing is ever changed under a player who is still in game.

Every character also has a save generation (`charxml.save_generation`), so an older copy of it can never be saved over
a newer one. It goes up when a world loads the character for play, on every save from that world and on every change
made here. A world only saves over the generation it loaded or last saved itself; anything else means someone newer
(another world, or the dashboard) saved the character since, so the save is refused and the newer data kept. That
covers a world that noticed a disconnect late after the player moved on, and a world still holding a character that
was edited here. A refused save is logged by the world and written to the audit log as `stale_save_refused` (so it
shows under the account's related data); that world saves nothing more for the character, and a player still
connected to it is disconnected (the client's save failure message) so they load the newer data.

- **Edit** (GM 8+, `characters_edit`): coins, U-score, level, and adding or removing items.
- **Edit XML** (GM 8+, `characters_edit_xml`, needs `enable_char_xml_upload=1`): replace the whole character XML. The
  upload is checked against everything the game assumes when it loads a character (required elements, numbers that
  must parse, known inventory types and mission states, items and missions that exist, unique item IDs and slots, the
  `acct` attribute matching the owner) and refused with the list of problems if the game couldn't load it. Suspicious
  content (contraband, stacks above the item's stack size, coins, level or U-score out of reach, a GM level above the
  account's) is shown as warnings and needs **Save anyway**; contraband marked *flag and remove* is taken out only if
  you tick the box, and either way it is flagged (kind *Contraband*) and the warnings are audited
  (`character_xml_warnings`).
- **History** (GM 3+, `characters_history`): earlier versions of the character. Each one shows what differs from
  now, can be downloaded as XML, and (with `characters_edit`) restored. The `character_snapshots` task saves every
  character that changed once a day; old snapshots are removed after `snapshot_days` (90), but each character always
  keeps its newest `snapshot_keep` (10).
- **Give it back** (GM 8+, `items_restore`): on the Economy page, trace an item and mail it back to the player. If
  the item no longer exists anywhere it comes back with its original ID.
- **Give back lost items** (GM 8+, `items_restore`; picking a snapshot needs `characters_history`): on the character
  page, pick a snapshot to see what the character had then and is missing now (per item, and every change per
  inventory), then tick items, or a whole item set, to mail them back. Mail works whether the player is online or not,
  and their saved data isn't touched. Only what is really missing is sent: items are counted across all inventories
  (moved or split stacks aren't lost), and what waits in their mailbox (including an earlier restore) or that someone
  else now holds (traded or mailed away) is taken off. Lost items nobody holds go back with their original IDs.

### Missions and progress

- **Missions** (everyone who can see the character): the character's missions and achievements with their tasks,
  progress, state and rewards. With `characters_missions` (GM 5+), staff can complete one (with or without its
  rewards), reset it so it can be done again, or give it even if its prerequisites aren't done, including any mission
  found by name or ID. If the player is in game their world does it exactly like the `/completemission`,
  `/resetmission` and `/addmission` commands, and tells them. If not, their saved data is changed the way the game
  would save it (they're disconnected from character select first and a snapshot is kept); rewards can only be given
  in game, so complete without rewards or wait until they're online. Everything is in the audit log.
- **Progress** (everyone who can see the character; players with `own_characters`): missions and achievements done
  out of those in the game, per group the game sorts them into and per zone, with what the game counts per zone
  (achievements earned there, coins, enemies, quick builds) and what the zone's summary tracks (collectibles, flags,
  pets). It is compared with the server's average and says how many characters have done less. The averages come
  from reading every character, so they are worked out in the background when first needed and kept for 6 hours.

### Related data

The account and character pages have a **Related** card with everything tied to them: characters, properties, pets,
friends, name requests, trades and mail, bug reports, economy flags, staff actions and recent chat, each shown only to
people allowed to see it.

## Moderation

### Review queue

**Review Queue** (GM 3+, `moderate_names`) holds character names waiting for approval, and for staff who may moderate
them, pet names (`moderate_pet_names`, GM 5+, also on their own **Pet Names** page) and properties made public
(`moderate_properties`, GM 5+). Approving or rejecting
tells the player in game if they're online. Rejecting asks for an optional reason, which the player sees on their own
account page next to what they asked for. The queues show only what the game itself would still review: rejected
properties and names whose player must pick a new one no longer appear.

Guild names the chat filter doesn't allow wait there too (`guilds_manage`, GM 5+). The **Guilds** page (Moderation)
lists every guild with its members and history; staff approve or reject a pending name (a rejected guild becomes
"Guild <id>"), rename a guild, remove a member or disband a guild. Online members see the change at once (the chat
server is told through master). See docs/Guilds.md.

Pet names show what kind of pet each one is (its icon and name from the CDClient), from the pet's LOT that the game
stores with the name (`pet_names.pet_lot`). Names set before that column existed get their LOT the next time the pet's
owner loads into a world; until then they show as Unknown.

### Moderation history, warnings and bans

Each account page has a **Moderation history** (GM 2+, `accounts_notes`): notes and warnings staff write, plus every
ban, mute and lock with its reason, including bans and mutes done with GM commands in game. A warning can also be
sent to the player in game.

A ban (GM 4+, `accounts_ban`) takes a reason and optionally a number of days. The player sees the reason when they try
to log in, and a temporary ban lifts itself at the next login after it ends (the `lift_expired_bans` task also lifts
ended bans every hour).

### Strikes

Something being rejected doesn't always mean the player did anything wrong, so strikes are a separate choice. When
rejecting a name, pet name or property, removing a leaderboard score or acting on a player report, staff with
`strikes_give` (GM 3+) can tick **Also give a strike on their account**; the dialog shows how many strikes the account
already has. Strikes can also be given by hand from the account page, and revoked (`strikes_revoke`, GM 5+): a revoked
strike stays on the record but stops counting. `strike_expiry_days` (Settings, Dashboard, Strikes) makes older strikes
stop counting; 0 (the default) keeps them forever.

The account page lists every strike with what it was for, the reason, who gave it and whether it still counts. Players
see their own (`own_strikes`), without staff names. Giving and revoking strikes is audited and goes to the
`moderation` webhook event.

**Strike thresholds.** In the same settings section you can have something happen on its own when an account reaches a
number of active strikes: `strike_warn_at` (a warning on the record, shown to the player if online), `strike_mute_at`
with `strike_mute_days` (3), and `strike_ban_at` with `strike_ban_days` (7; 0 bans permanently). 0 switches a step off,
and all are off until you set them. For example warn at 2, mute for 3 days at 3, ban for 7 days at 5. Right after a
strike is given, the highest threshold reached is applied the same way the account page's buttons do it (moderation
history, audit log, `moderation` webhooks, the player disconnected when banned), and the moderator who gave the strike
is told what happened. A step is applied once for its number of strikes: a fourth strike after the mute at three does
nothing more, and neither does reaching three again after a strike was revoked, as long as the strike that set the mute
off still counts towards `strike_expiry_days`. A mute or ban never shortens a longer one, and nothing happens to an
account the moderator couldn't manage themselves (staff at or above their level).

### Player reports

Players report someone from the game's **Report Abuse** window (in the help menu, or **Report** on another player's
name): another player, a model they point at, or a property. These arrive on the **Player Reports** page (GM 2+,
`player_reports_view`, under Moderation) with who sent them, what they wrote, where they were, and who the report is
about: the reported player, or the owner of the property or of a model placed on it. Reports about another player used
to be filed as bug reports; they now come here, and model and property reports, which the server used to drop, are
kept too. Each world takes at most 3 reports a minute and 20 a day from one account; more are dropped (with a line in
the world's log), so nobody can flood the queue.

Staff with `player_reports_manage` (GM 3+) close a report with **Act on it** (what was done, and optionally a strike on
the reported account, with the same dialog as rejecting a name) or **Dismiss**. Muting, warning or banning is done
from the account page as usual. Both are audited; acting on a report goes to the `moderation` webhook event.

### Linked accounts

Account pages have a **Linked accounts** card (GM 3+, `accounts_links`): other accounts with the same play key, the same
email address, or that logged in to the game from the same network address, with how many reports are about the account.

Login addresses are recorded by the auth server at each successful game login while `log_login_addresses` is on (the
default; Settings, Players and access). An address is personal data: the dashboard never shows it, only that two
accounts share one, and an address an account hasn't used for `log_login_address_days` (90) is deleted by the nightly
log pruning. A shared address can also be a family, a school or a public network, so treat it as a hint. Turn
`log_login_addresses` off if you don't want addresses kept (and mention it in your privacy notice if you do).

### Mail

The **Mail** page (GM 3+, `characters_mail`, under Moderation) lists every in-game mail, newest first and live: when it
was sent, the sender (a character, the game, or staff from the dashboard) and receiver linked to their character
pages, the subject, the attachment (icon, name, count; waiting or claimed) and the state (unread, read, deleted).
Click a subject for the whole mail: body, accounts, attachment item ID, subkey and item data. Filter by state, by a character (sent or
received) or an account ID; the search box matches the stored subject, body and names. `/mail#character=<id>`,
`#account=<id>` and `#state=deleted` open it filtered.

Mail from the game stores locale keys (`%[MissionEmail_12_subjectText]`); the dashboard shows them as the client does,
from the client's `locale/locale.xml` (needs `client_location`), with the stored text under **Stored text**.

When a player deletes a mail in game, the row is kept and marked deleted with the time (`mail.deleted_at`). The game never
shows it again (mailbox, unread count, claiming); staff see it on the Mail page and in the character's **Mailbox**. The
character's owner sees only what is still in their mailbox. Deleting a character still removes its mail. API:
`POST /api/tables/mail`, `GET /api/characters/:id/mail`.

### Chat filter

The **Chat Filter** page (GM 5+, `chat_filter_manage`, under Moderation) decides which words players below GM 2 may use
in chat. The filter's files: `chatplus_en_us.txt` (client `res` folder) lists the words normal chat may use,
`blocklist.dcf` (next to the servers, hashes only) the words best friends' free chat may not. Approved character names
also count as allowed. Words are compared lower case, without `! ? ; . ,`. Changes apply at once in running worlds and in
the chat server's web chat; servers that start later read them. Changes are audited and go to the `moderation` webhook
event.

The page has three sections:

- **Test a message**: type a message, pick normal or best friends' free chat, and see whether it would be sent and why,
  word by word (in the file, allowed or blocked here, a character name, not allowed, in the blocked words file). Each
  word has a Block, Allow or Remove button.
- **Staff lists**: **Blocked** words are stopped in all chat, even where a file allows them; **Allowed** words are usable
  in normal chat. Search, filter by list, 50 per page. Block, Allow (or move to the other list) and Remove each open a
  confirmation that shows where the word stands now and, for Block and Allow, the recent chat it changes (players' chat
  containing it that would have been stopped, or stopped messages containing it; the newest 1000 messages with the
  text; needs `chat_view`).
- **Word files**: how many words each file has, and the words of `chatplus_en_us.txt` (needs `client_location`),
  searchable, 200 per page, coloured by whether a staff list also has them. Click one to test it. **Copy file words into
  Allowed** copies every word of the file into the Allowed list once (words already on a list stay as they are); the file
  is not changed, and a word removed from Allowed is still allowed by the file until it is blocked.

**Re-apply in running worlds** loads the lists again in every world; changes already do this. API:
`GET /api/chat_filter`, `GET /api/chat_filter/test?message=&chat=normal|free`, `GET /api/chat_filter/lookup?word=`,
`GET /api/chat_filter/check?word=&allowed=`, `GET /api/chat_filter/files?search=&start=`, `POST /api/chat_filter/words`,
`POST /api/chat_filter/words/delete`, `POST /api/chat_filter/import`, `POST /api/chat_filter/reload`.

### AI moderator helper

Staff with `ai_suggest` (GM 3+) get a **Suggest** button on player reports, chat log messages (and "Suggest for this
player's recent chat" under the Character filter), pending character and pet names, and open economy flags. It asks
Claude (Anthropic's API) to draft a suggestion: an action (dismiss, note, warn, strike, mute or ban for some days;
approve or reject for names), a short reason written for the player, a paragraph for staff citing the evidence (chat
lines, earlier strikes and moderation, shown with the suggestion), and how confident it is. It is only a draft: **Use
this** fills in the usual dialog (the reject or act dialog, the flag review) and **Fill in on the account page** fills in
the note, mute, ban or strike form there, and staff check it and apply it themselves. Nothing is ever applied on its
own, and nothing the helper writes reaches players unless staff send it.

It is off until set up in Settings, Dashboard, AI moderator helper: `ai_helper_enabled`, `claude_api_key` (never shown
back or logged), `claude_model` (`claude-sonnet-5` by default; any model ID works), `ai_helper_rules` (your code of
conduct, sent with every request), and limits so it can't run up a bill: `ai_helper_per_minute` (5),
`ai_helper_per_day` (200, per UTC day, for the whole dashboard), `ai_helper_max_tokens` (2000 per request) and
`ai_helper_timeout` (60 seconds; busy or failing answers are retried up to three times). The Moderation page shows how
many suggestions are left today.

Each request sends the item, the player's chat from `ai_helper_chat_minutes` (10) before and after it, their strikes,
notes, earlier reports and name decisions, and only what the asking staff member could see themselves. It never sends
emails, addresses, passwords or account names. What players wrote is marked as data the model must not obey, and answers
that aren't exactly the expected JSON, cite things that weren't sent, contain links for the player or repeat the
helper's instructions are thrown away. Every suggestion is stored with the item (asking again about an unchanged case
shows the stored one at no cost; **Ask again** asks anew) and audited as `ai_suggest` with who asked, the model and the
tokens used. To try it without a key, run `python3 tests/dWebTests/mock_claude_api.py --port 8765` and set
`claude_api_base=http://127.0.0.1:8765`.

### Chat log and chat bridges

The **Chat Log** page (GM 3+, `chat_view`) shows what players say, newest first and live: zone chat, messages sent in
from the dashboard or a chat bridge, and messages the chat filter stopped (marked; nobody saw them). Team and guild chat
are there too for staff with `chat_private` (GM 8+), whispers for staff with `chat_dms` (GM 8+). Filter by channel,
character, account, zone or one world of a zone, a time range, text, or "only what the filter stopped"; **Context** on a
row shows the conversation around it, **Conversation** opens its whisper, team or guild history, and **Account** goes to
the sender's account to mute, warn or ban them. The Related card on account and character pages has their recent
messages, and the character page's Chat card links to their chat log, whispers, team chat and chat flags.

Every message is written by the server that sends it, as it is sent: zone chat by the world server, whispers, team and
guild chat by the chat server. Each row has the channel, the sender's character and account, the recipient (whispers),
the guild (guild chat), the team (team chat; team IDs start from the chat server's start time, so they are not reused
after a restart), the zone, instance and clone, the time, and the filter result: `blocked` (zone chat the filter
stopped; nobody saw it) and `filtered` (the filter found words it doesn't allow; whispers, team and guild chat are
delivered either way). Rows are deleted after `log_chat_days` by the Log pruning task, on its own database connection.

The histories, paged on the server (newest page first, **Older** for the one before):

- **Guild chat**: one guild's chat, from **Guild chat** on the guild's card on the Guilds page (`chat_private`).
- **Team Chat** (Moderation menu, `chat_private`): teams that talked, most recent first, with who talked; filter by a
  character to see only their teams.
- **Whispers** on a character (`/characters/<id>/whispers`, `chat_dms`): everyone they whispered with, most recent
  first; pick one to read the conversation. Opening a conversation (also from a flag or the Context view) is audited
  as `chat_dms_view`.

**Chat flags.** Tick messages on the Chat Log or a history (shift-click picks a range) and **Flag** them (`chat_flag`,
GM 3+): pick who it is about (one of the senders) and write a note. The messages must be from one conversation. The
flag keeps a copy of them and up to 10 messages before and after, so it survives pruning. The **Chat Flags** page
(Inbox) is the queue, open ones first, with a badge for open flags; filter by status, character or account. A flag
shows its copy of the chat (flagged messages highlighted), links to the character, the account (mute, warn, strike,
ban), player reports about them, their other flags and the conversation, and its history: who flagged it, who changed
its status, note or linked player report, and comments. Reviewers (`chat_flag_review`, GM 3+) mark it actioned or
dismissed (or open again). Everything is also in the audit log (`chat_flag`, `chat_flag_review`). Flags on team, guild or
whisper chat show their messages only to staff who may read that chat.

Settings: `log_chat` (on), `log_private_chat` (on), `log_chat_days` (30), and `chat_bridge_filter` (on: messages sent
into the game go through the chat filter like players' do).

**Bridging chat to Discord or another service.** Make an account for the bot with a GM level that has `chat_view`,
`chat_send` and `api_access`, make an API key on its account page with just `chat_view` and `chat_send` (and,
for the WebSocket, no address limit), and:

- Read: `GET /api/chat?after=<last id>` with `Authorization: Bearer <key>` returns the messages newer than that id,
  oldest first, and `last_id` for the next call (`zone` and `instance` narrow it to one zone or one world). Or connect
  to the `/ws` WebSocket with the same header and subscribe to the `chat_message` topic to get each message as it's
  said (`{"event":"subscribe","subscription":"chat_message"}`).
- Write: `POST /api/chat/send` with `{"message": "hi", "name": "Bob", "label": "Discord"}` (and `"zone": 1200`, plus
  `"instance"` for one world of it, to post in one place only). Players see it as `[Discord] Bob: hi`; the label is
  always shown (`Web` when left out), so a bot can't pass for a player.

`tools/chat-bridge` is an example bridge that uses only this API (Python, with Discord, console and webhook adapters
and a place to add others). It relays zone chat only by default, skips what the chat filter stopped and its own
messages, rate limits both ways, and resumes from the last id after a restart or a dropped WebSocket. See its README
for setup and a systemd unit. A message the chat filter stops when sent in with `/api/chat/send` is not marked as
stopped in the Chat Log.

### Leaderboards

**Leaderboard moderation** (GM 5+, `leaderboards_manage`): on the Leaderboards page, remove one character's score (for
example a cheated one) or clear a whole board. Both go in the audit log.

Columns and their names are the ones the game shows for the activity's leaderboard type, and the order is the game's.
A foot race's Time is the time left on the race clock at the finish, so more is better; Monument Race and racing times
are time taken, so less is better.

## Properties

The property page's model list shows each model's icon: for a player-built model (or a car or rocket) the icon the UGC
server made of it (the item's icon until it has), with its UGC state and a link to it on the UGC page. The character
page's inventories (models, vault models and the brick building ones included) show creations the same way.
The **UGC** page (`/ugc`, `properties_view`) finds creations by owner, account, property, name, LOT or id and says where they are (it
replaced the UGC Search page): see docs/UgcServer.md.

**Today's Top Properties** (Properties page, permission `feature_properties`, default GM 5). The game's news screen has
four "Today's Top Properties" slots, one per small property world: Block Yard, Nimbus Rock, Chantey Shanty and Raven
Bluff (from the client's PropertyTemplate and PropertyEntranceComponent tables). A slot only displays what the server
sends it, so it can show a property of any property world, small or medium (every world a property entrance leads to,
such as Avant Grove or Nimbus Isle). Only approved public properties are shown, and never the same one in two slots.

- *Full auto* shows the four approved public properties with the most reputation across every property world, in slot
  order. The per slot settings are kept for when you switch back.
- *Per slot*: each slot has a *location* (the property world it shows a property of; the slot's own world by default)
  and is *Auto* (the location's approved public property with the most reputation, the default), a *picked* property
  (search by the property's name, description or owner; only approved public properties of the location, and not one
  another slot already picked), or *Empty* (locked). Picks are placed first; the Auto slots then take, in slot order,
  the best property of their location that isn't shown yet, so two Auto slots on one world show its first and second.
  A pick that later becomes private, rejected, or loses its owner falls back to Auto the same way. A slot with nothing
  left to show stays locked.

"Showing" on each slot is what players see. The game's tooltip on a slot always names the slot's own world (and its
description), even when it shows another world's property; a property without a name shows the slot world's default
name. Players see changes the next time they open the news screen, within 30 seconds. Changes are audited as
`feature_property`. API: `GET /api/featured_properties`, `POST /api/featured_properties {full_auto}`,
`GET /api/featured_properties/:template/candidates?location=&search=`,
`POST /api/featured_properties/:template {location, mode, property_id}`.

### Property rent

Off unless `property_rent_enabled` is on (Settings, Gameplay, Property rent). Owners then pay rent for their
properties like live: each property world costs its PropertyTemplate's `minimumPrice` every `rentDuration`
`durationType` (type 1 counts days, the rest months of 30 days; every property world is 1 month, Block Yard is free),
unless the **Property Rent** page (World & Economy; `properties_view` to see it, `property_rent_manage` (GM 8) to
change it) sets another price (0: free) or period for the world.

Rent is taken from the owner's coins a few seconds after their character loads in any world; paying moves the due
date one period on from then (missed periods aren't charged later) and the owner gets a mail receipt. If they don't
have enough coins they get a mail, and once `property_rent_grace_days` (3) have passed since the rent was due the
property is made private. It can't be made public or best friends only again until the rent is paid (the owner is
told in chat when they try), and a property world that loads with overdue rent makes itself private too. Nothing on
the property changes. The property's page shows the rent last charged and when the next is due.

### Property reputation

Properties earn reputation from visitors; it orders the in-game property lists and the news screen's **Today's Top
Properties**, and the showcase. Live first gave 1 point per minute a visitor spent on a property (each
PropertyTemplate's `reputationPerMinute` is 1) and later replaced that with an unpublished algorithm because it was
easy to farm. The client only displays the number the server sends, so the server decides. This one keeps "time
other people spend on the property" as the signal and makes farming it expensive (settings under Gameplay, Property
reputation; the code is `dCommon/PropertyReputationRules.h`):

1. **Only other people.** Visitors are counted per account, so the owner's characters never count and one account
   with several characters on the property is one visitor. Accounts linked to the owner's (same play key, email or
   login address, as on the Linked accounts panel; `property_reputation_ignore_linked`) and staff
   (`property_reputation_ignore_staff`) don't count either.
2. **A minimum visit.** Nothing for the first `property_reputation_min_visit` seconds (120, WorldConfig's
   `propertyReputationDelay`), so hopping in and out does nothing.
3. **Active minutes.** Once a minute, a visitor who moved at least 2 units since the last minute
   (`property_reputation_require_activity`) earns `reputationPerMinute` × `property_reputation_multiplier` points,
   for at most `property_reputation_max_minutes` (30) minutes per visit. An idle alt parked on a property earns nothing.
4. **Diminishing returns for regulars.** A visitor who gave the property reputation on *d* of the last
   `property_reputation_repeat_days` (30) days earns 1 / (1 + `property_reputation_repeat_falloff` × *d*) as much
   (0.5: half on the third day, a fifth after eight). Fractions carry over between the minutes of a visit.
5. **Daily caps** (UTC days): one account gives one property at most `property_reputation_visitor_daily_cap` (30)
   points a day, and a property gets at most `property_reputation_daily_cap` (300) a day from everyone.

With the defaults a stranger spending 20 active minutes gives 19 points; the same account coming back every day for a
week gives about 100 in total, less than seven different visitors staying 20 minutes each. Small servers can raise the
multiplier. Nothing decays. What each account gave each property per day is kept in `property_reputation_visits`
(`GET /api/properties/:id/reputation` sums the last 30 days); the character's own reputation (from missions) is not
changed.

### Properties in 3D

**Open 3D view** on a property page (or *View in 3D* next to a model) shows every placed model on the zone's terrain,
drawn like the game draws it: the whole zone, with its ground textures blended by the zone file's blend maps. The blue
line is the build area from the zone file, where the owner may place models. Pick the level of detail, turn terrain and
shadows on or off, and go full screen. Click a model, or pick it from the list, to see its details and behaviors: each
state's strips with their trigger and actions. **Interact**, **Attack** and the chat buttons fire a model's triggers
and play its behaviors the way the server runs them (moves, waits, smashing and rebuilding, chat bubbles, state
changes); sounds, spawned enemies and drops are listed in the log. Actions the server doesn't support are shown in
red. Terrain needs `client_location` to be set.

**Layers.** The property 3D view (and the showcase's) fills the window with its panels scrolling inside; its **Layers**
tab has the same switches as World 3D where they apply: detail, Terrain, Scenery, Sky, Fog (off by default), Placed
models, Build area, Shadows and Hidden objects (off by default), remembered per account.

**Generated models.** When the UGC server has made a player-built model, the property 3D view draws it from the model
the UGC server made (the NIF the game client gets), and builds the rest from their LXFML as before; switch
**Generated models** off in Layers to build every model from its LXFML. The switch shows once the property has a
made model; a model's Selected tab says which it is drawn from, with its UGC state. See docs/UgcServer.md.

**Scenery.** The property 3D view (and the showcase's) draws the zone around the property as the game does: every
scene object's model and the zone's sky, from the game client's files (needs `client_location`). Models load nearest
first; the detail setting picks the model level of detail, how far objects are drawn (1400/800/450 units), texture
sharpness and a memory budget. Switch it off with *Scenery*. The 3D world view draws the same as its *Models* layer
(on by default, Medium detail), plus the terrain's flairs. Converted models are cached in memory (64 MB) and in
`dDashboardServer/scenery_cache` next to the server (at most 512 MB); nothing needs ImageMagick.
Whether a texture's alpha makes a model see-through follows the shader the game draws it with (the render component's
shader, or for a multishaded model the `S05__`-style tag in each part's name), not only the .nif: the LEGO shaders
lay the texture over the vertex colors and terrain meshes and LEGO items ignore its alpha, so their textures' alpha
channels (often gloss or leftover masks) don't punch holes in them.

**Game shaders.** Scenery, sky and flairs are drawn with ports of the client's shaders (`res/shaders/*.fx`) to WebGL
(`static/js/game-shaders.js`), one program per technique family and variant, shared by every mesh that uses it. Which
technique a shader (mapShaders `gameValue`) uses is one table, `NifFile::TechniqueFor` (dCommon); the manifests carry
it as `techniques` (gameValue -> family, eShaderLook bits, texture alpha, eTechniqueFlag bits). A gameValue the
table lacks is drawn as LEGO, as the client falls back to it. The math runs on sRGB values as Direct3D 9 did, with the
zone's sun, ambient, upper hemisphere and specular colors from the scene files, blended as the camera moves between
scenes.

| Family | gameValues | Drawn as |
| --- | --- | --- |
| LEGO (LEGOPPLighting) | 4, 5, 12, 14, 19, 20, 22, 25-31, 48, 50, 53, 72, 88, 92 | hemisphere lit sun + ambient, fresnel rim, N.H^320 specular, default reflection cube x0.05; texture over vertex colors by its alpha (decal) or times them (Item, Masked NonDecal, FaceCreate); Emissive/SuperEmissive (vertex alpha x emissive red), Glow, Grayscale, NoAmbient, AnimUV |
| Basic (BasicShaders, AlphaAsAlpha) | 7-11, 13, 15-18, 23, 24, 32-39, 49, 52, 54-65, 68, 70, 73, 80-87, 91, 93, 94, 105-108 | (sun x N.L + ambient) x vertex color per vertex, clamped once as the COLOR0 output (or unlit), x texture; AlphaAsAlpha both sides (its techniques set Cullmode none, so cards made to be seen from one side show their backs from outside a playable area); AlphaBlend without depth writes, AlphaTest cut out, Additive; material alpha for the AnimAlpha ones; two layer blends and adds |
| Metal (Metallic.fx) | 98, 99, 100 | 0.7 N.L^4 + 0.3, plus the client's metal cube (polished or brushed, with the brushed noise in object space) tinted by the vertex color, specular |
| Clear Plastic | 6 | default reflection cube, fresnel of the lit grey, tight highlight; see-through facing the eye |
| Ocean (Distortion) | 69, 89, 90, 95, 101 | three texture layers, each warped by the one before, lit (or unlit x2, or FX with the emissive alpha window) |
| Flat Surf | 78 | texture lit, alpha from a second lookup |
| BrickWater | 51 | hemisphere lit vertex color, fresnel, specular |
| Darkling | 75-77, 102-104 | dark texture on the second UV set over the base look by a window of vertex alphas (material emissive r/g/b); Specular uses LEGO lighting, Structure multiplies |
| Terrain meshes | 2, 3, 97 | texture x vertex color x (sun x N.L + ambient); Rim Light adds a rim; Diffuse Map Only doubles the texture |
| Flairs (Flair.fx) | the flair manifest | (0.85 sun + ambient) x tint, clamped once; the tint is the terrain file's color byte / 255 times the flair model's own vertex colors |
| Sky (Skydome.fx) | the scene's sky | texture x vertex color, unlit, moving by its texture transform |
| Not drawn | 21, 71, 74, 79, 96 | model footprints, post-processing, drop shadows, Undefined |
| Fixed function | -1 | sun x N.L + ambient with the material color and NiVertexColorProperty |

Moving textures (the AnimUV, ScrollingUV and Distortion shaders, the sky) scroll as the .nif's texture transform
controllers say. Fog (Layers, off by default: the views look from further out than the game's camera) mixes in the
zone's fog color from its near to far distance, terrain included. Approximated or left out: shadows, the LEGO parallax
and normal maps (BrickWater), Ocean's layer motion (the client moves its layers by timers of its own), the shiny glint
(a band moving up the object), Reveal and FadeUp masks (drawn fully shown), Glow's color (the shader's default cyan),
Powerups, Orb, TV Screen and Head Icon effects (drawn as LEGO or unlit), Flair's distance fade and movement. The
client's environment textures come from `/api/scenery/env/:name` (`reflection`, `polished`, `brushed`,
`brushedNoise`). Placed player models are drawn by the viewer's own lighting.

The views ask for a zone's manifest with the conversion format they're written for (`?format=6`,
`scenery-core.js` `SCENERY_FORMAT`, which follows `Scenery.cpp` `FORMAT_VERSION`), since browsers keep manifests for up
to a day. A manifest without `techniques` (an older server's) is drawn with the viewer's own lights and its
`textureAlpha` table instead of guessing every shader as LEGO, which laid see-through textures over white vertex
colors (white trees, rocks and water).

Converting a model the caches don't have yet (a big "glom" file takes a moment) happens on a few worker threads, so
the dashboard keeps answering everything else meanwhile: the route hands the request to a worker (`Web::Defer`) and
the web thread sends the answer when it is ready. Flairs and small models (up to 256 KB) go first, and one of the
threads only takes those, so the grass around the camera never waits behind a big model; models of 4 MB and more wait
behind smaller ones. A model asked for twice at once (two viewers) is converted once. When a zone's scenery or flair
manifest is asked for, the zone's models are also converted ahead of time onto the disk cache, flairs and smallest
first, at the detail its viewer last used: only when nothing else waits, on at most half of the threads besides the
flairs' one, stopping when nobody has viewed the zone for 90 seconds or the disk cache is three quarters full (it
never evicts for this). The number of threads is `scenery_workers` in `dashboardconfig.ini` (Settings > Dashboard >
Web server; 0, the default, picks half the CPU cores, 2 to 4; read at startup).

The same threads build a zone's data the first time it is viewed (its terrain chunks and layers, scene objects,
paths, scenery and flair manifests, deflated bodies) and convert terrain textures with ImageMagick, so opening a new
zone doesn't hold up the rest of the dashboard; what is built already is answered at once. Each is built once, even
when asked for twice at the same time. Workers never query the CDClient, read settings or touch the network: the
tables they need (ZoneTable, render components, flairs, object names, terrain textures) are read at startup.
Endpoints:
`/api/properties/:id/scenery`, `/api/world3d/:zone/scenery`, `/api/world3d/:zone/flairs`,
`/api/scenery/:zone/mesh/:asset?lod=`, `/api/scenery/:zone/texture/:asset/:slot?lod=`. The world view's other data:
`/api/world3d/:zone/scene` (objects and scenes), `/terrain_chunks` and `/terrain_layers` (the terrain file; sent
deflated when the browser takes it, about a tenth of the size) and `/paths`.

### Property showcase

**Property Showcase** (`/showcase`) lets players browse each other's properties that the owner made public and a
moderator approved: name, owner's character name, description, world, number of models and reputation, with a search,
a world filter and sorting by reputation, recent changes or name. **Look around in 3D** opens the same viewer as above,
read only (no model downloads, no link to the owner's character). Private, friends-only, waiting and rejected properties
never show; one that is rejected or made private drops out within seconds.

It needs the `showcase_view` permission (every account by default; raise it on the Permissions page to take it away).
Set `showcase_public=1` (Settings, Dashboard, Public pages) to let visitors who aren't signed in see it too; they're
rate limited per address.

## Economy reports

**Economy & Map** (GM 3+, `reports_view`) shows where coins, U-score and items come from and go, what players did each
day, trades and mail between players, item traces by object ID, duplicate scans, and a world map of where things
happen. Every table can be downloaded as CSV.

Items that change hands in a trade or in mail between players get a new object ID, as on live (a whole item keeps its
data; part of a stack joins the other player's stack). Trades and mail record the old and the new ID ("Received as").

**Trace Item** follows an item through every recorded trade, mail and move between a player's own inventories. The item
gets a new object ID at each of these (as on live), so enter any of its IDs: the trace goes back to the first recorded
ID and forward to the latest (up to 200 hops) and shows a timeline (time, how, from → to, world, count, coins, old → new
ID), where each ID is now and which IDs still exist. When a stack was split, every part is shown (the other parts
greyed out). A stack that went into one the receiver already had is marked *merged*; that stack's older history is left
out unless you switch on *Also show the history of stacks it merged into*. Items that later joined the item's stack are
listed as *joined the stack*. Gaps are marked: mail sent before sending was recorded, a hop that starts with someone the
previous hop didn't give the item to, or a hop without a new ID. Nothing before the first recorded hop is known (loot,
vendors, missions, or trades from before the ledger). You can open a trace by clicking an item in a character's
inventory, an ID in the Object ID column on Trades & Mail, or an item in the trades and mail on character and account
pages. Inventory moves are recorded for traces only and don't show on Trades & Mail. **Give it back** mails the item
under its latest ID when there is only one.

The **World Map** tab draws a heat map over the zone's minimap (or its terrain) for one kind of event: enemy kills, item
drops, coin drops, player deaths (listed by what killed them), coins players dropped when they died, smashables
smashed by players and quickbuilds completed. Events are kept in 4×4 unit squares; with the whole zone in view they
are merged into bigger squares so each one shows, and split again as you zoom in.

Properties run one copy of their zone per property, so the reports keep each property apart (by the property's clone,
which the world records with every event and statistic). On the **World Map**, property zones are listed under
*Properties* ("Block Yard properties"); pick *All properties* to see them together (different builds on the same ground,
so only good for totals) or one property to see its own map, drawn with its build area (dashed) and its placed models
(blue squares), with its owner, links to the property page and 3D view, its totals and (with `players_history`) who was
seen there. The **Activity** tab has a *Where* picker (everywhere, all properties, one property zone's properties, one
property, one world), and its per-world table folds properties into *All properties* and one row per property zone. The
3D view's heat map has the same property picker. Events recorded before properties were told apart show as *Unknown
property* rather than under a property. A property's page links to its economy data, and a character's page to what
happened on their properties.

Powerups are recorded too: where they drop and where players pick them up (a teammate can pick up the same drop). The
World Map and Activity tab show them by what they restore (health, imagination, armor, or their effect, such as speed),
worked out from each powerup's skills in the client database.

The **Activity** tab charts those events per day, plus daily totals of the statistics every character keeps
(smashables smashed, quickbuilds and missions completed, pets tamed, times smashed, distance travelled, races
finished, ...), overall and per world. Statistics the client reports on its own are left out, since a modified client
could send anything. The *Include staff* switch applies to the statistics; world events always include everyone.

World servers keep running totals and write them every few seconds (added into daily rows, so storage grows with
active players and places, not with events). Each night, shortly after midnight UTC, [scheduled tasks](#scheduled-tasks):

- flag unusual coin income, item spikes and duplicated items (thresholds: `anomaly_*` settings; the duplicate scan
  can be switched off with `economy_duplicate_scan=0`),
- merge daily detail (and player statistics) older than `economy_detail_days` (180; map data: `economy_map_days`, 90)
  into monthly totals,
- delete trades and mail older than `economy_transfer_days` (730),
- delete log rows older than `log_activity_days`, `log_command_days`, `log_audit_days`, `log_cheat_detection_days`,
  `log_chat_days`, `log_login_address_days`, `health_days` and `log_task_days` (0 keeps everything).

Flags can be marked dismissed or actioned (`reports_review_flags`, GM 3+); `reports_run_checks` (GM 8+) runs the
checks by hand.

A duplicate is the same item (the same LOT) under one object ID in more than one place. Old data sometimes gave one
object ID to two different items: those show separately as **Object ID collisions**, are not dupes and are not flagged.
The nightly check dismisses any open duplicate flag whose ID turns out to be a collision, recorded as the system. Trace
shows the item for each copy.

Object ID collisions come from saves made before item IDs were made unique (character version below 8, the `<lvl cv>`
value in the character's XML). The next time such a character logs in, the game gives all its items new, unique IDs,
which clears the collision. Each copy shows its character's version and whether it gets a new ID at the next login, and
each collision shows whether it resolves on login and whose login is needed. The nightly economy check dismisses
duplicate flags that are really collisions, with a note saying who needs to log in, and raises an "ID collision" flag
only for collisions that logging in will not fix (both characters already migrated, or a copy is in mail). A real
duplicate (the same item twice) is still flagged even when one copy is on an old save: the login gives it a new ID but
keeps both copies, so check it before then.

### Contraband

The **Contraband** page (World & Economy; viewing needs `reports_view`, changing the list `contraband_manage`, GM 8)
lists items players shouldn't have. Pick an item with the item search (or type its LOT), give a reason and choose
what happens when a character has one:

- **Flag**: an economy flag of kind *Contraband* is added (Economy & Map, Flags; and the character's related data),
  to review like any other flag.
- **Flag and remove**: the same flag, and the item is taken away.

World servers check every inventory, the vault included, when a character loads (one flag per item), and every
item a player receives from loot, trades, mail, vendors and so on (one flag per character, item and day). Moving an
item between a player's own inventories doesn't count. Before removing items at login the world keeps a snapshot of
the character (reason "before contraband removal"), so **Give back lost items** on the character page can return
them if an item was listed by mistake; every removal is in the audit log as `contraband_removed`. Players get a mail
(at login) or a chat message (when received) saying what was removed and why, unless `contraband_notify_players` is
off. Staff accounts (GM level above 0) aren't checked unless `contraband_ignore_staff` is off. Every change to the
list is audited and running worlds load it again straight away.

### Saved views and report emails

On the Economy page, **Views** saves the current tab, range, staff toggle and item filter under a name, so you can go
back to it in one click (links look like `/reports#view=Name`). Ranges stay relative ("last 7 days"). A saved view can
be emailed to you every day or every Monday around 07:00 UTC with the totals, top sources, most created items, top
earners and open flags; that needs email set up and a confirmed address on your account.

## Public pages and players

### Public server status

Off by default. With `public_status=1` (Settings, Dashboard, Public pages) anyone can see, without signing in:

- `/status`: a page with whether the server is up, players per world, uptime, server health and the top leaderboard
  places;
- `/api/public/status`: the same as JSON for server lists (any site may fetch it);
- `/status/widget`: a small box other sites may show in an iframe
  (`<iframe src="https://dashboard.example.com/status/widget" width="260" height="80"></iframe>`; add `?theme=light`
  for light pages). Turn it off with `public_status_widget=0`.

What it shows is up to you: `public_status_players` (players per world, on), `public_status_names` (character names by
world, off; staff are never named but are counted), `public_status_uptime` (how long it's been up, and how much of the
last day and week, from the Server Health samples; on), `public_status_health` (whether login and chat are up and how
many worlds run; on), `public_status_leaderboard_top` (places per leaderboard, 3; 0 leaves them out) and
`public_server_name`. It never shows account names, addresses, instance or property IDs. The status is worked out at
most every `public_status_cache_seconds` (60) however many people ask, and each address is limited to 120 requests a
minute.

The public pages are `/status` and the [property showcase](#property-showcase) with `showcase_public=1`; both are off
by default. The sign-in page links to the ones that are on.

### For players

Every account can sign in to the dashboard (unless `min_dashboard_gm_level` keeps them out). Players see their own
account and characters, including:

- their properties and whether each is approved, with the moderator's reason if it was rejected;
- their pets and whether each name is approved, waiting or rejected (with the reason);
- their character name requests and what happened to them;
- their characters' missions and progress, and their strikes;
- **Leaderboards** for every activity, ranked the same way the game ranks them, with their own characters
  highlighted and a search to find anyone's place;
- the [property showcase](#property-showcase) of other players' approved public properties.

Players who forget their password can reset it by email (when set up) or, with two-factor login, with a
[recovery code](#forgotten-passwords).

## Vanity

The **Vanity** page (GM 8+, `vanity_manage`) looks after the extra NPCs, props and plaque texts the world servers add
from the **vanity files**, `vanity/*.xml` next to the server binaries, and shows what the worlds load with the
[scheduled events](#scheduled-events) that change them. Vanity is off entirely while `disable_vanity` is set.

**Files & NPCs** tab:

- **Vanity files**: the worlds start at `root.xml` and load each file that a loaded file switches on
  (`<file name="summer.xml" enabled="1"/>`), each once; any file can include others. The list shows them that way,
  with a switch for each include, and files nothing includes apart (**Include** adds one to `root.xml`, switched off).
  Each file says whether it is loaded and, when not, why. A badge says how many events use it (the tooltip says which,
  and how: as their overlay file, or switching it on or off), and *on now* / *off now* when an event that is on loads
  it differently from what the files say. **New file** makes an empty one, switched off in the file you pick.
- **NPCs**: pick a file to edit its includes and NPCs: each NPC's name, look (LOT and equipment), what it says, its
  config and where it stands (zone, position, facing, chance, scale; or one of its locations at random). Nothing is
  written until **Save file**, which checks every NPC the way the worlds read them and keeps the previous file as
  `.bak`. An NPC that an event replaces or takes out has a badge linking to that event.
- **Vanity events** lists the events with vanity changes, and **New vanity event** starts one.
- **Plaque texts**: `TESTAMENT.md` (the plaque by the Nimbus Station launch pad), `CREDITS.md` and `INFO.md` (the
  `/credits` and `/info` commands).

**Respawn in game** makes every running world load the vanity files again, with the events that are on.

### Vanity changes in events

A vanity part of a scheduled event changes the vanity NPCs only while the event is on: a Halloween look for October,
a werewolf on full-moon nights. The vanity files themselves are never changed:

- **File switches** turn vanity files on or off, as `root.xml`'s switches do, wherever a loaded file names them. A file
  switched on loads even if nothing names it (after the others, by name); a file switched off doesn't load, nor do the
  files only it includes. Halloween can switch `halloween.xml` on and `summer.xml` off.
- **Overlay file**: a vanity file that `root.xml` doesn't load, edited on the Files & NPCs tab like any other (naming one
  that doesn't exist yet makes it empty when the event is saved). Its NPCs are laid over the others: an NPC with the
  same name as a vanity NPC replaces it, all of its locations, so to give an NPC a different outfit, lines or place,
  copy it into the overlay file and change it there. Unnamed objects (props, decorations) are always added.
- **NPCs to take out**: names of vanity NPCs that are gone while the event is on.

When several events that are on switch the same file or change the same NPC, the one with the higher priority wins.
The Scheduled Events page lists the events that could meet like that, and warns when events that are on now do.

The world servers read the events each time they spawn their vanity NPCs (when they start, and on a respawn): they
load `root.xml` with the file switches of the events that are on, then lay each event's overlay file and removals over
that, in priority order. So a world that starts during an event has it. When a vanity part starts or ends (its event
turns on or off, or it is changed while on), the dashboard respawns the vanity NPCs in every running world; the old
ones are removed first.

### Preview

The **Preview** tab shows what the worlds would load at any date and time, or with the events you pick: the events in
the order they are laid on, which vanity files are loaded and why (as the files say, or switched on or off by which
event, highlighted where an event made the difference), how many NPCs there are with and without events, the conflicts
between events, anything that couldn't be read, and the merged vanity XML. The dashboard and the worlds load vanity
the same way (one function in the server code), so the preview is what the worlds get. **Preview** on an event opens it
with just that event.

## API

Everything the dashboard does is available as a JSON API. Make an **API key** on your account page and send it as
`Authorization: Bearer <key>`. The **API** page lists every endpoint your GM level can use, and each can be tried
out there. Using the API at all needs the `api_access` permission (every account by default); raise it on the
Permissions page to turn the API off for players or lower staff levels.

### API keys

Each key has a name and note, an optional expiry, and a scope: either **all of your permissions** or only the ones you
pick (grouped by category like the Permissions page; you can only pick permissions you have). A key never does more
than its owner can do *right now*: every request is checked against the owner's current account and GM level and
against the key's scope, so demoting, banning or locking the owner narrows or stops their keys at once.

- Routes guarded by a permission need it in the key's scope. The self and rank rules apply twice: the key also needs
  `self_tools` / `self_items` / `self_moderation` in its scope to act on its owner's own account or characters, and
  `manage_equal_rank` to act on accounts of the owner's level (even when the owner is GM 9, who needs neither).
  The few routes guarded only by a GM level above 0 need a key with all of its owner's permissions.
- **Read-only** keys only make GET requests (and the DataTables queries under `/api/tables/`, which are POSTs that
  only read).
- **Only from addresses**: exact addresses or prefixes ending in `.` or `:` (`10.0.0.`). With `behind_proxy=1` the
  proxy's `X-Forwarded-For` is used. Keys limited to some addresses can't open the WebSocket (its address isn't
  checked there).
- **Only these paths**: path prefixes such as `/api/chat`; include `/ws` to allow the WebSocket.
- **Limits**: requests a minute (default `api_key_rate_limit`, 120; up to 6000) and optionally requests a UTC day.
  Every answer carries `X-RateLimit-Limit` and `X-RateLimit-Remaining` (and `X-Quota-Limit` / `X-Quota-Remaining`
  with a quota); going over answers `429` with `Retry-After`. Requests, last use and last address are shown in the
  key list; they are written to the database once a minute, not on every request.
- WebSocket subscriptions follow the scope too (`chat_message` needs `chat_view`, and so on).
- Keys can never be used to sign in, change the account's password, email or two-factor login, sign out sessions,
  or make, rotate or revoke keys. Those need a signed-in browser session, so a leaked key can't make itself new keys
  or lock its owner out.
- Only a hash of each key is stored; the key is shown once when it is made. **Rotate** gives a key a new secret and
  keeps its settings; **Revoke** stops it for good. **Sign out everywhere**, a password change or a password reset
  stops every key made before it (rotate a key to use it again).
- Staff with `api_keys_manage` (GM 8 by default) see other accounts' keys on their account pages and can revoke
  them, following the rank rules. Nobody can make keys for someone else.
- Making, rotating and revoking keys is audited (and sent to `security` webhooks). What is done with a key is audited
  as `user (key name)`, and requests a key was refused (scope, read-only, address, path, limits) are audited as
  `api_key_denied`, at most once a minute per key and reason.

`POST /api/auth/token` (for scripts written for the old API tokens) now makes a key named "API token" with all of the
caller's permissions, valid for the given `days`. API tokens made before API keys existed keep working, with the
account's full permissions, until they expire (at most a year) or the account signs out everywhere.

## Developer tools

Two pages for people writing scripts and content, under **Developer Tools** in the menu (GM 8+ each).

### Game message inspector

**Message Inspector** (`dev_message_inspector`) shows the game messages one online player's client sends and
receives, live, and keeps every capture so it can be opened again later: the message's name, direction, time, the
object it is for, its size, its bytes (up to 2 KB of each) and its fields where the server reads the message with a
typed struct (skills, projectile impacts, using and picking up things).

- Start a capture with the character's name, for 30 seconds up to 15 minutes, optionally only messages in one
  direction, only some messages (`REQUEST_USE, 154`: names or numbers) or never some (`READY_FOR_UPDATES`). It stops
  by itself at the end, or with **Stop**. At most 4 captures run at once, one per character.
- The world server the player is in captures, and sends what it caught through master in batches four times a second.
  It keeps at most 200 messages a second per capture (and 1000 waiting); the viewer marks where it left some out.
- When the player changes zones or logs out the capture waits, and carries on in whichever world they turn up in
  until its time is up. Each world also stops a capture on its own at the time limit, so one can't be left running
  even if the dashboard goes away. With no capture running, the world servers do no extra work.

**Saved captures.** The dashboard saves each capture in the database while it runs (every second): who was captured
(character and account), who started it, its filters, when it started and ended and why, the worlds it was captured in
(zone, instance, clone), and each message with its raw bytes and decoded fields. A capture keeps at most 100,000
messages and stops when it reaches that. If the dashboard stops, a capture that still has time left carries on when it
starts again (messages sent while it was down are lost); others are marked ended. The **Saved captures** tab lists
them, newest first, filtered by character, account, the staff member who started it and date, sortable by start,
character, staff, messages or size, a page at a time. Click one to open it in the viewer.

**The viewer** is the same for running and saved captures:

- Filter by message (part of a name or an ID, several separated by commas, `!NAME` to hide one), direction, object (or
  click an object ID in the table) and text in the decoded fields or hex bytes; **Only decoded** hides messages
  without fields. **Counts by message** shows how many of each message were sent each way; click one to show only it.
- **Follow newest** keeps the newest message in view (scrolling up stops it); **Pause** holds new messages back while
  you look and **Resume** adds them. **Time since the start** shows times relative to the start of the capture.
- The table only draws the rows in view, so captures with tens of thousands of messages stay quick. A yellow edge marks
  a message with others left out just before it; a blue line marks where the player moved to another world.
- Click a message (or move with the arrow keys) for its fields, where it was, and a hex dump with offsets and an ASCII
  column (hovering a byte highlights its character). **Copy hex** and **Copy JSON** copy the message.
- **Download JSON** saves the capture and all its messages. **Delete** removes a finished capture and its messages
  (with a reason for the audit log).

Captured messages are player data: only staff with `dev_message_inspector` can see the page, the saved captures and
their messages. Starting, stopping, downloading and deleting captures are audited, and so is opening a finished capture
someone else started (at most once an hour per staff member and capture).

Saved captures are kept for `inspector_session_days` (default 30; 0 keeps them) and, when all of them together take more
than `inspector_max_mb` (default 1024 MB; 0 for no limit), the oldest are deleted first. Both are under Settings, Data
retention. The **Message capture pruning** task applies them every night (Tasks page); running captures are never
deleted. Messages are stored as the world captured them (bytes plus the fields it decoded), so decoders added to the
world server later only apply to new captures.

### CDClient browser

**CDClient Browser** (`dev_cdclient`) is a raw viewer for the game's CDClient database (`resServer/CDServer.sqlite`),
for checking the values the server reads. It lists every table; each table is paged (50 rows), sortable by any column,
searchable across all columns and filterable by column (`=`, `!=`, `<`, `<=`, `>`, `>=`, contains, starts with, null,
not null). Values that point at other rows (a LOT, a loot matrix, a skill, a mission, a zone, an emote, ...) link to
the target table filtered to that ID; in `ComponentsRegistry`, `component_type` shows the type's name and
`component_id` links to the component's row. The view is in the URL, so it can be linked; `/cdclient#/object/<LOT>`
(or `mission`, `skill`, `loot_matrix`, ...) opens the target table filtered to that ID.

It shows values only. For worked-out views (loot odds, mission rewards, behavior trees) use lu-explorer.

It is read-only. Tables and columns come from the database itself; the page never sends SQL, only which table, column
and value to look at.

## Checking a build

`tests/smoke/dashboard_smoke_test.py` starts the whole server on a throwaway copy of your SQLite database, checks that
every server comes up and connects, opens every page and API route, checks that players and moderators are refused
what they shouldn't reach, and that live updates arrive. It then stops the server. It takes about half a minute:

```sh
python3 tests/smoke/dashboard_smoke_test.py --build build           # a copy of the configured database
python3 tests/smoke/dashboard_smoke_test.py --build build --fresh   # an empty database
```

For a MySQL server, start it yourself and use `--attach http://127.0.0.1:2006 --user <GM 9 account>` with the
password in `SMOKE_PASSWORD`. It creates two test accounts and deletes them again.

The MySQL parity tests (`tests/dDatabaseTests`) run every migration on a fresh MySQL/MariaDB database and a fresh
SQLite file and check that both database implementations return the same results. They need a running MySQL or MariaDB
server and are skipped unless `DLU_TEST_MYSQL_HOST` is set:

```sh
DLU_TEST_MYSQL_HOST=tcp://127.0.0.1:3306 ctest --test-dir build -R Parity   # or run build/dDatabaseTests
```

`DLU_TEST_MYSQL_USER` (default `root`), `DLU_TEST_MYSQL_PASSWORD` (default empty) and `DLU_TEST_MYSQL_DATABASE`
(default `dlu_parity_test`) are optional. That database is dropped and created again, so its name must contain `test`.
