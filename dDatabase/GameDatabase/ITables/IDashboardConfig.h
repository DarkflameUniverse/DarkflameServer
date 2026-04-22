#ifndef __IDASHBOARDCONFIG__H__
#define __IDASHBOARDCONFIG__H__

#include <optional>
#include <string>
#include <string_view>

/**
 * Interface for Dashboard configuration table.
 * Stores key-value configuration settings for the Dashboard server.
 */
class IDashboardConfig {
public:
	
	struct DashboardConfig {
		std::string key;
		std::string value;
	};
	
	// Get a configuration value
	virtual std::optional<std::string> GetDashboardConfig(const std::string_view key) = 0;
	
	// Set a configuration value
	virtual void SetDashboardConfig(const std::string_view key, const std::string_view value) = 0;
};

#endif  //!__IDASHBOARDCONFIG__H__
