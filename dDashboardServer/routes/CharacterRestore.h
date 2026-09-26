#pragma once

/**
 * Giving a character back what it lost since one of its snapshots: the difference from now (per inventory, with item
 * names), and mailing chosen items (or a whole item set) back. Mail reaches the player whether they are in game or
 * not, and their saved data is never touched. Only what the character is still missing is sent: what it holds now,
 * what waits in its mailbox and what someone else now holds are taken off first (InventoryRestore.h).
 */
void RegisterCharacterRestoreRoutes();
