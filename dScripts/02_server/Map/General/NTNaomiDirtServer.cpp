#include "NTNaomiDirtServer.h"

namespace {
	std::map<int32_t, std::string> VisibilityMissionTable = {
		{1253, std::string("Dirt_Clouds_Sent")},
		{1276, std::string("Dirt_Clouds_Assem")},
		{1277, std::string("Dirt_Clouds_Para")},
		{1283, std::string("Dirt_Clouds_Halls")}
	};
};

void NTNaomiDirtServer::OnStartup(Entity* self) {
	SetGameVariables(VisibilityMissionTable);
}
