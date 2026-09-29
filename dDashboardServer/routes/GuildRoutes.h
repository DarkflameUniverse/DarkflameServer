#pragma once

/**
 * Guilds (docs/Guilds.md): the list, each guild's members and history, and moderation (approve or reject a name that
 * waits for review, rename, disband, remove a member). Needs guilds_manage. The dashboard writes the database, audits
 * the change, adds it to the guild's history and asks the chat server (through master) to tell the online members.
 */
namespace GuildRoutes {
	void RegisterRoutes();
}
