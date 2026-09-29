#include "HolidayEvents.h"

#include "CDClientManager.h"
#include "CDEventGatingTable.h"
#include "dConfig.h"
#include "Game.h"

#include <array>
#include <ctime>
#include <string>

bool HolidayEvents::IsActive(const std::string_view eventName, const int64_t unixTime, const std::string_view* enabledEvents, const size_t enabledCount) {
	if (eventName.empty()) return false;
	for (size_t i = 0; i < enabledCount; i++) {
		if (enabledEvents[i] == eventName) return true;
	}
	return CDClientManager::GetTable<CDEventGatingTable>()->IsEventActive(eventName, unixTime);
}

bool HolidayEvents::IsActive(const std::string_view eventName) {
	static constexpr std::array<const char*, 8> KEYS = { "event_1", "event_2", "event_3", "event_4", "event_5", "event_6", "event_7", "event_8" };
	std::array<std::string, 8> settings;
	std::array<std::string_view, 8> enabled;
	for (size_t i = 0; i < KEYS.size(); i++) {
		settings[i] = Game::config ? Game::config->GetValue(KEYS[i]) : "";
		enabled[i] = settings[i];
	}
	return IsActive(eventName, static_cast<int64_t>(std::time(nullptr)), enabled.data(), enabled.size());
}
