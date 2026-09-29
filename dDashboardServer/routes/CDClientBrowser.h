#pragma once

/**
 * The CDClient browser (developer tool): a raw viewer for the game's CDClient database. Any table can be paged, sorted,
 * searched and filtered by its columns; columns that point at other tables (see CDClientSchema.h) link to the rows they
 * point at. Read-only; the browser never sends SQL.
 */
void RegisterCDClientBrowserRoutes();
