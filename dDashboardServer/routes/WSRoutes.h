#pragma once

#include "json.hpp"
#include <cstdint>

void RegisterWSRoutes();
void BroadcastDashboardUpdate();

// Account broadcasts
void BroadcastAccountUpdate(uint32_t accountId);
void BroadcastAccountsTableUpdate();
void BroadcastAccountListChanged();

// Character broadcasts
void BroadcastCharacterUpdate(uint32_t characterId);
void BroadcastCharactersTableUpdate();
void BroadcastCharacterListChanged();

// Property broadcasts
void BroadcastPropertyUpdate(uint32_t propertyId);
void BroadcastPropertiesTableUpdate();
void BroadcastPropertyListChanged();

// Play Key broadcasts
void BroadcastPlayKeyUpdate(uint32_t playKeyId);
void BroadcastPlayKeysTableUpdate();
void BroadcastPlayKeyListChanged();

// Bug Report broadcasts
void BroadcastBugReportUpdate(uint32_t bugReportId);
void BroadcastBugReportsTableUpdate();
