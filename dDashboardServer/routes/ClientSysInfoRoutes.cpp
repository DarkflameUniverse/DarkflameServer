#include "ClientSysInfoRoutes.h"

#include "ClientSysInfoView.h"
#include "Database.h"
#include "GeneralUtils.h"

#include <algorithm>
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
			JsonSuccess(reply, { {"rows", rows}, {"caveats", ClientSysInfoView::Caveats()}, {"trust", ClientSysInfoView::Trust()}, {"showsIp", showIp} });
		});

	Route(eHTTPMethod::GET, "/api/client_sysinfo/spread", Perm("client_sysinfo"),
		"How the system info clients report is spread across players, from each account's newest row (approximate: as reported by the "
		"client): {spread: {accounts, os, video, memory, processors, clientOs: [{label, count}]}, caveats}",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonSuccess(reply, { {"spread", ClientSysInfoView::Spread(Database::Get()->GetLatestClientSysInfo(SPREAD_ACCOUNTS))},
				{"caveats", ClientSysInfoView::Caveats()}, {"trust", ClientSysInfoView::Trust()}, {"showsIp", Can(context, "logs_audit")} });
		});

	Route(eHTTPMethod::POST, "/api/tables/client_sysinfo", Perm("client_sysinfo"),
		"Every client system info row across accounts for browsing (DataTables), as reported by the client; rows as "
		"/api/accounts/:id/client_sysinfo plus account_name. Search: part of the account name or video card. Body adds {latest (bool: only each "
		"account's newest row), account (ID)}. Columns: last seen, account, logins, Windows version, video card, processors, memory, client build, "
		"first seen. ip only with logs_audit",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			IClientSysInfo::SysInfoQuery query;
			query.search = request->search;
			query.latestOnly = body->value("latest", false);
			if (const auto it = body->find("account"); it != body->end()) {
				if (it->is_number_unsigned()) query.accountId = it->get<uint32_t>();
				else if (it->is_string()) query.accountId = GeneralUtils::TryParse<uint32_t>(it->get<std::string>()).value_or(0);
			}
			query.order = static_cast<IClientSysInfo::eSysInfoOrder>(std::min<uint32_t>(request->orderColumn,
				static_cast<uint32_t>(IClientSysInfo::eSysInfoOrder::FIRST_SEEN)));
			query.ascending = request->orderAsc;
			query.offset = request->start;
			query.limit = std::clamp<uint32_t>(request->length, 1, 500);
			const bool showIp = Can(context, "logs_audit");
			nlohmann::json rows = nlohmann::json::array();
			for (const auto& row : Database::Get()->ListClientSysInfo(query)) rows.push_back(ClientSysInfoView::RowJson(row, showIp));
			IClientSysInfo::SysInfoQuery all;
			all.latestOnly = query.latestOnly;
			all.accountId = query.accountId;
			JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", Database::Get()->CountClientSysInfo(all)},
				{"recordsFiltered", Database::Get()->CountClientSysInfo(query)}, {"data", rows}, {"caveats", ClientSysInfoView::Caveats()},
				{"trust", ClientSysInfoView::Trust()}, {"showsIp", showIp} });
		});
}
