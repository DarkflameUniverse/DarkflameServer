#ifndef __DASHBOARDWEB_H__
#define __DASHBOARDWEB_H__

#include <cstdint>
#include <string>

namespace DashboardWeb {

	// Initialize the web server and configure routes using blueprints
	void Initialize(uint32_t port);

	// Process pending web requests (call each frame/tick)
	void Update();

	// Stop the web server
	void Stop();
};

#endif // __DASHBOARDWEB_H__
