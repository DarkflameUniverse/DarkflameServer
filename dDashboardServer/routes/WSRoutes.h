#pragma once

#include <cstdint>
#include <string>

/**
 * Live updates pushed to dashboard browsers over WebSocket.
 *
 * Topics (each gated by GM level at subscribe and send time):
 *  - dashboard_update  (GM 0): server status, online players and totals
 *  - table_changed     (GM 1): {table, id?} - a table's contents changed, clients refetch through the API
 *  - moderation_counts (GM 3): sizes of the moderation queues for sidebar badges
 *
 * table_changed never carries row data, so a client only ever sees data it can fetch with its own permissions.
 */
void RegisterWSRoutes();

// Called periodically from the main loop. Pushes server status and detects changes made outside the
// dashboard (game servers, other tools) by diffing a cheap database snapshot.
void BroadcastDashboardUpdate();

// Notify clients that a table changed. id is optional and lets detail pages refresh only when relevant.
void BroadcastTableChanged(const std::string& table, const std::string& id = "");

struct DataChanged;

// Rows a game server just wrote (DATA_CHANGED from a world, via master), pushed on as table_changed events.
// Only known table names are passed on.
void BroadcastDataChanged(const DataChanged& changed);
