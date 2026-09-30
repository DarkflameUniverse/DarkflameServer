#include "ClientSysInfoRoutes.h"

#include "ClientSysInfoView.h"
#include "Database.h"
#include "RouteUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr uint32_t HISTORY_ROWS = 200;
	constexpr uint32_t SPREAD_ACCOUNTS = 100000;
}

void ClientSysInfoRoutes::RegisterRoutes() {
	Route(eHTTPMethod::GET, "/client_sysinfo", Perm("client_sysinfo"), "Client system info: the spread across players, as reported by the client",
		[](HTTPReply& reply, const HTTPContext& context) {
			RenderPage(reply, context, "client_sysinfo.jinja2", "client_sysinfo");
		});

	Route(eHTTPMethod::GET, "/api/accounts/:id/client_sysinfo", Perm("client_sysinfo"),
		"The system info an account's client sent at login, as reported by the client (compatibility values from old Windows calls, not "
		"necessarily the real hardware): {rows: [...] newest first (at most 200), caveats: {field: text}, showsIp}. ip only with logs_audit",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid account");
			const bool showIp = Can(context, "logs_audit");
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& row : Database::Get()->GetClientSysInfo(*accountId, HISTORY_ROWS)) rows.push_back(ClientSysInfoView::RowJson(row, showIp));
			JsonSuccess(reply, { {"rows", rows}, {"caveats", ClientSysInfoView::Caveats()}, {"showsIp", showIp} });
		});

	Route(eHTTPMethod::GET, "/api/client_sysinfo/spread", Perm("client_sysinfo"),
		"How the system info clients report is spread across players, from each account's newest row (approximate: as reported by the "
		"client): {spread: {accounts, os, video, memory, processors, clientOs: [{label, count}]}, caveats}",
		[](HTTPReply& reply, const HTTPContext&) {
			JsonSuccess(reply, { {"spread", ClientSysInfoView::Spread(Database::Get()->GetLatestClientSysInfo(SPREAD_ACCOUNTS))},
				{"caveats", ClientSysInfoView::Caveats()} });
		});
}
