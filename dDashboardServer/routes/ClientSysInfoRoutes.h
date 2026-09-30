#pragma once

/**
 * Client system info (client_sysinfo): what each account's game client reported about its system at login, on the
 * account page, and the spread across players on its own page. Values are as reported by the client and often
 * compatibility defaults, not the real hardware (ClientSysInfoView::Caveats). Addresses only with logs_audit.
 */
namespace ClientSysInfoRoutes {
	void RegisterRoutes();
}
