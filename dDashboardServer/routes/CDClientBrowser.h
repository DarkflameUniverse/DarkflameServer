#pragma once

/**
 * The CDClient browser (developer tool): search and read the game's CDClient database. Any table can be paged and
 * filtered by its columns, and objects, loot matrices and tables, missions, skills, behaviors and activities have
 * their own views that follow the links between tables (see CDClientSchema.h). Read-only; the browser never sends SQL.
 */
void RegisterCDClientBrowserRoutes();
