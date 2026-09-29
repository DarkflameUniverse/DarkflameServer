# Issue tracker (experimental branch)

Upstream issues this branch works on, with the commits and their state. Commit messages on this branch name issues
as "issue 1234" (no hash), so nothing is linked on GitHub until a change is proposed upstream; this file is the index.
Some earlier commits still say "Fixes #N"/"Refs #N" (already linked; history not rewritten).

State: **done** = fixed on this branch, needs an in-game check; **partial** = part of it, see notes.

| Issue | Title | State | Commits |
|---|---|---|---|
| 159 | BUG: Brick-by-brick models are deleted instead of put away | done | `6ce261c5` fix: brick by brick and model placement work the way the client expects |
| 185 | BUG: Assembly Engineer Fortress Knockback | partial | `2d76c81b` feat: server side knockback for AI moved objects |
| 225 | ENH: "bind_ip" config option | done | `e36f894f` feat: bind_ip setting for the server sockets |
| 257 | EH: Crux Prime shields stun instead of knockback | done | `2d76c81b` feat: server side knockback for AI moved objects |
| 536 | BUG: Pet Bouncer incomplete implementation | done | `4b13c98f` fix(pets): pet bouncers wait for the owner and cost imagination<br>`bcc47ca2` wire: add the Help game message |
| 537 | BUG: Pet Digs missing interaction step and imagination cost | done | `eb7370ba` fix(pets): pet digs wait for the owner and cost imagination<br>`bcc47ca2` wire: add the Help game message |
| 539 | BUG: Client thinks pet is active after automatically de-spawning | done | `f53ca094` fix(pets): the backpack lets go of a pet that went away on its own |
| 546 | BUG: (Most?) spawned Pets don't play their summoning/hibernating Animation/VFX/Audio | done | `d1e0f215` fix(pets): summoned pets play their spawn animation and effect |
| 547 | BUG: Pet Taming Mini-Game uses poor/invalid positions | done | `6819c670` fix(pets): place the taming minigame where live did |
| 596 | BUG: Crux Prime Computer does not respawn if it is killed by an Area of Effect attack | done | `b3166865` fix: Crux Prime dropship computer respawns and no longer crashes |
| 636 | BUG: Reputation system not working correctly on Properties | done | `d95eab12` feat: property reputation from visitors, resistant to farming |
| 637 | BUG: Today's Top Properties not showing any properties | done | `d95eab12` feat: property reputation from visitors, resistant to farming |
| 639 | ENH: Implement saving checks to prevent overwriting of saves | done | `341ce89a` feat: refuse stale character saves with a save generation |
| 691 | Tracking Issue: Hardcoded Content | partial | `fec4159e` refactor: modular build items and root part come from ModularBuildComponent<br>`d4a1a993` refactor: power-up statistics come from the power-up's pickup skill<br>`e44eaf18` refactor: item set passive abilities come from the CDClient and item scripts |
| 746 | Tracking Issue: Missing Scripts | partial | `4ad7ddd9` feat: add missing force field, jetpack NPC and Skullkin volume scripts |
| 764 | BUG: Driving the wrong way does not warp you back | done | `6febb6d6` fix(racing): put racers going the wrong way back on the track |
| 943 | ENH: Add config option for charging property rent | done | `680615ba` feat: optional property rent |
| 960 | ENH: Make use of `minNumRequired` when deleting items | done | `02108055` feat(inventory): enforce DeletionRestrictions when deleting items |
| 1021 | BUG: Crux Prime daily mission objectives for smashables don't always complete | partial | `b9d5ef99` fix: Skullkin drill credits the player who breaks it |
| 1113 | ENH: Add data validation to user editable strings in .ini's | partial | `040d7ec0` fix: ignore whitespace around config keys and values |
| 1127 | BUG: Enemies at Cavalry Hill take 1 damage from too far away on spawn | done | `5d3c2e6f` fix: filter physics volumes like the client's collision groups |
| 1129 | BUG: Vendor selling window does not replicate live behavior | done | `c759bd1a` fix(vendor): keep 27 buyback items and drop the oldest |
| 1179 | BUG: enemies aren't affected by speed alterations | done | `86ee6de5` fix: slows and speed buffs change enemy movement speed |
| 1332 | ENH: make saving code safer | partial | `4443d981` fix: read character flags the way they are written again<br>`d671d266` fix: character xml load and save no longer crash on bad data |
| 1428 | BUG: enemies, summons etc stay aggro'd onto other entities that have died | done | `363e02e9` fix: enemies and summons drop targets that have died |
| 1563 | ENH: Admin defined "contraband" detection and removal | done | `b2403b09` feat: contraband list with flagging and optional removal |
| 1565 | BUG: Removing Brick from Property | done | `6ce261c5` fix: brick by brick and model placement work the way the client expects |
| 1632 | BUG: Getting kicked/disconnected while in Brick-By-Brick building mode causes the currently selected model to be wiped | done | `6ce261c5` fix: brick by brick and model placement work the way the client expects<br>`9256b31b` feat(db): bbb_autosave table for the client's BBB autosave |
| 1971 | BUG: Avant Gardens Survival boundary incorrectly triggers | partial | `5d3c2e6f` fix: filter physics volumes like the client's collision groups |
| 2035 | BUG: Unbounded AMF3 associative-array entries use predictable hashing (potential HashDoS) | done | `8a2ccb1a` fix: bound AMF3 decoding and stop hashing client keys |

## Waiting on other work

- Pets 166 (untamed dragons and skunks don't use their skills): not fixed. Their combat AI, NPC combat skill and
  hostile faction are set up by DamagingPets; nothing wrong could be found without watching it in game.
- Pet scripts' group TODOs: the panda and crab now leave their spawn groups (`68bb1dc6`); the dig and object pet
  scripts' live versions are not shipped with the client, so theirs stay.
- Property behaviors 803, floating models 983: not started.
