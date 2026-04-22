#include "DashboardHelpers.h"

namespace DashboardHelpers {

DataTablesParams ParseDataTablesParams(const crow::request& req) {
    DataTablesParams p;
    try {
        if (req.url_params.get("draw")) p.draw = std::stoi(req.url_params.get("draw"));
        if (req.url_params.get("start")) p.start = std::stoi(req.url_params.get("start"));
        if (req.url_params.get("length")) p.length = std::stoi(req.url_params.get("length"));
        if (req.url_params.get("order[0][column]")) p.orderColumn = std::stoi(req.url_params.get("order[0][column]"));
        if (req.url_params.get("order[0][dir]")) p.orderDir = req.url_params.get("order[0][dir]");
    } catch (...) {
        // ignore parse errors, return defaults
    }
    return p;
}

crow::json::wvalue CreateDataTablesResponse(int draw, uint32_t recordsTotal, uint32_t recordsFiltered, const crow::json::wvalue::list& data) {
    crow::json::wvalue resp;
    resp["draw"] = draw;
    resp["recordsTotal"] = recordsTotal;
    resp["recordsFiltered"] = recordsFiltered;
    resp["data"] = data;
    return resp;
}

bool RescueCharacter(const uint64_t characterId, const uint32_t zoneId) {
    // Minimal stub: not implemented here. Return false to indicate no-op.
    return false;
}

} // namespace DashboardHelpers
