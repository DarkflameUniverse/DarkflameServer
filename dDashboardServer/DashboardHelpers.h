#pragma once
#include <crow.h>
#include <string>

namespace DashboardHelpers {

struct DataTablesParams {
    int draw{0};
    int start{0};
    int length{10};
    int orderColumn{-1};
    std::string orderDir{"asc"};
};

// Parse common DataTables GET params from the request
DataTablesParams ParseDataTablesParams(const crow::request& req);

// Create a DataTables response object
crow::json::wvalue CreateDataTablesResponse(int draw, uint32_t recordsTotal, uint32_t recordsFiltered, const crow::json::wvalue::list& data);

// Rescue character stub (real logic may be project-specific)
bool RescueCharacter(const uint64_t characterId, const uint32_t zoneId);

}
