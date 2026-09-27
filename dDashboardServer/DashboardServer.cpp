#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <csignal>
#include <memory>

#include "CDClientDatabase.h"
#include "ConfigSync.h"
#include "CDClientManager.h"
#include "Database.h"
#include "dConfig.h"
#include "Logger.h"
#include "dServer.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "ServiceType.h"
#include "MessageType/Master.h"
#include "MasterPackets.h"
#include "PacketDispatcher.h"
#include "Game.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "Diagnostics.h"
#include "Locale.h"
#include "Web.h"
#include "Server.h"

#include "ServerState.h"
#include "APIRoutes.h"
#include "StaticRoutes.h"
#include "DashboardRoutes.h"
#include "WSRoutes.h"
#include "AuthRoutes.h"
#include "PlayerActions.h"
#include "AccountRoutes.h"
#include "ClientAssets.h"
#include "MaintenanceRoutes.h"
#include "ReportRoutes.h"
#include "SecurityRoutes.h"
#include "WebhookRoutes.h"
#include "RouteUtils.h"
#include "EconomyJobs.h"
#include "Scheduler.h"
#include "BackupRoutes.h"
#include "ReportViews.h"
#include "CharacterTools.h"
#include "CharacterRestore.h"
#include "MissionTools.h"
#include "CharacterProgress.h"
#include "ServerRoutes.h"
#include "LeaderboardRoutes.h"
#include "RequireAuthMiddleware.h"
#include "Permissions.h"
#include "VanityRoutes.h"
#include "ChatRoutes.h"
#include "Strikes.h"
#include "ModerationTools.h"
#include "ModeratorHelper.h"
#include "LiveWorld.h"
#include "Scenery.h"
#include "Workers.h"
#include "ReportRoutes.h"
#include "WorldView.h"
#include "ClientAssets.h"
#include "DashboardRoutes.h"
#include "LiveEventRoutes.h"
#include "ChallengeRoutes.h"
#include "Inspector.h"
#include "CDClientBrowser.h"
#include "master/MessageCapture.h"
#include "PublicRoutes.h"
#include "Showcase.h"
#include "ContrabandRoutes.h"
#include "UgcRoutes.h"
#include "PropertyRentRoutes.h"
#include "FeaturedProperties.h"
#include "PasswordRecovery.h"
#include "SettingsRoutes.h"
#include "SettingsHistory.h"
#include "Announcements.h"
#include "EventsCalendar.h"
#include "InstanceLoad.h"
#include "WorldView.h"
#include "PrometheusMetrics.h"
#include "Background.h"
#include "master/DashboardMessages.h"
#include "master/DataChanged.h"
#include "EmailService.h"
#include "AuthMiddleware.h"
#include "DashboardAuthService.h"
#include "Totp.h"
#include "Alerts.h"
#include "DashboardAuthService.h"
#include "AuthTokenHandler.h"
#include "JWTUtils.h"
#include "GeneralUtils.h"
#include <fstream>
#include <filesystem>

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

// Define global server state
namespace ServerState {
	ServerStatus g_AuthStatus{};
	ServerStatus g_ChatStatus{};
	std::vector<WorldInstanceInfo> g_WorldInstances{};
	std::mutex g_StatusMutex{};
}

namespace {
	dServer* g_Server = nullptr;
	// The server list is fetched again every so often: it corrects player counts and drops worlds that crashed
	// without saying so
	std::chrono::steady_clock::time_point g_NextServerListRequest{};
	constexpr auto SERVER_LIST_INTERVAL = std::chrono::seconds(30);

	std::string GetZoneDisplayName(uint32_t mapID) {
		if (mapID == 0) return "Character Select";
		std::string key = "ZoneTable_" + std::to_string(mapID) + "_DisplayDescription";
		const auto& name = Locale::GetPhrase(key);
		if (!name.empty()) return name;
		return "Zone " + std::to_string(mapID);
	}

	/**
	 * Load the JWT signing secret: jwt_secret from config if set, otherwise a random secret generated
	 * on first start and kept in a file next to the binary so sessions survive restarts.
	 */
	bool InitializeJWTSecret() {
		const auto configured = Game::config->GetValue("jwt_secret");
		if (!configured.empty()) {
			if (configured.size() < 32) {
				LOG("jwt_secret must be at least 32 characters");
				return false;
			}
			JWTUtils::SetSecretKey(configured);
			return JWTUtils::HasSecretKey();
		}

		const auto secretPath = BinaryPathFinder::GetBinaryDir() / "dashboard_jwt_secret";
		std::string secret;
		if (std::ifstream in(secretPath); in) std::getline(in, secret);

		if (secret.size() < 32) {
			secret = JWTUtils::GenerateSecret();
			if (secret.empty()) {
				LOG("Failed to generate a JWT secret");
				return false;
			}
			std::ofstream out(secretPath, std::ios::trunc);
			out << secret;
			out.close();
			std::error_code ec;
			std::filesystem::permissions(secretPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
			LOG("Generated a new dashboard JWT secret at %s", secretPath.string().c_str());
		}

		JWTUtils::SetSecretKey(secret);
		return JWTUtils::HasSecretKey();
	}

	// Which property a property instance is (clone id), for the world list
	void AddPropertyDetails(WorldInstanceInfo& info) {
		if (info.cloneID == 0) return;
		// The list is refreshed every 30 seconds; look each property up once
		static std::map<std::pair<uint32_t, uint32_t>, WorldInstanceInfo> cache;
		const auto key = std::make_pair(info.mapID, info.cloneID);
		if (const auto it = cache.find(key); it != cache.end()) {
			info.propertyId = it->second.propertyId;
			info.propertyName = it->second.propertyName;
			info.ownerId = it->second.ownerId;
			info.ownerName = it->second.ownerName;
			return;
		}
		try {
			const auto property = Database::Get()->GetPropertyInfo(info.mapID, info.cloneID);
			if (!property) return;
			info.propertyId = std::to_string(property->id);
			info.propertyName = property->name;
			info.ownerId = std::to_string(property->ownerId);
			if (const auto owner = Database::Get()->GetCharacterInfo(property->ownerId)) info.ownerName = owner->name;
			cache[key] = info;
		} catch (const std::exception&) {}
	}

	void OnServerList(const MasterPackets::ServerListResponse& list, const SystemAddress&) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		ServerState::g_AuthStatus.online = list.authOnline != 0;
		ServerState::g_AuthStatus.lastSeen = std::chrono::steady_clock::now();
		ServerState::g_ChatStatus.online = list.chatOnline != 0;
		ServerState::g_ChatStatus.lastSeen = std::chrono::steady_clock::now();

		ServerState::g_WorldInstances.clear();
		for (const auto& instance : list.instances) {
			WorldInstanceInfo info;
			info.mapID = instance.mapID;
			info.instanceID = instance.instanceID;
			info.cloneID = instance.cloneID;
			info.players = instance.players;
			info.ip = instance.ip.string;
			info.port = instance.port;
			info.isPrivate = instance.isPrivate != 0;
			info.zoneName = GetZoneDisplayName(info.mapID);
			AddPropertyDetails(info);
			ServerState::g_WorldInstances.push_back(info);
		}

		LOG_DEBUG("Received server list: auth=%s chat=%s worlds=%u",
			list.authOnline ? "online" : "offline",
			list.chatOnline ? "online" : "offline",
			static_cast<uint32_t>(list.instances.size()));
	}

	void OnServerInfo(const MasterPackets::ServerInfo& serverInfo, const SystemAddress&) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		switch (serverInfo.serverType) {
		case ServiceType::AUTH:
			if (serverInfo.ip.string == "offline") {
				ServerState::g_AuthStatus.online = false;
			} else {
				ServerState::g_AuthStatus.online = true;
				ServerState::g_AuthStatus.lastSeen = std::chrono::steady_clock::now();
			}
			break;
		case ServiceType::CHAT:
			if (serverInfo.ip.string == "offline") {
				ServerState::g_ChatStatus.online = false;
			} else {
				ServerState::g_ChatStatus.online = true;
				ServerState::g_ChatStatus.lastSeen = std::chrono::steady_clock::now();
			}
			break;
		default:
			break;
		}
	}

	void OnWorldReady(const MasterPackets::WorldReadyInfo& ready, const SystemAddress&) {
		const LWOMAPID zoneID = ready.zoneID;
		const LWOINSTANCEID instanceID = ready.instanceID;

		std::lock_guard lock(ServerState::g_StatusMutex);
		WorldInstanceInfo info;
		info.mapID = zoneID;
		info.instanceID = instanceID;
		info.cloneID = ready.cloneID;
		info.players = 0;
		info.ip = ready.ip.string;
		info.port = ready.port;
		info.isPrivate = ready.isPrivate != 0;
		info.zoneName = GetZoneDisplayName(zoneID);
		AddPropertyDetails(info);
		// Master can report a world more than once (in the server list and when it becomes ready): replace, don't add
		auto& instances = ServerState::g_WorldInstances;
		const auto existing = std::ranges::find_if(instances, [&](const WorldInstanceInfo& w) { return w.mapID == zoneID && w.instanceID == instanceID; });
		if (existing != instances.end()) {
			info.players = existing->players;
			*existing = info;
		} else {
			instances.push_back(info);
		}

		LOG("World ready: zone %i instance %i", zoneID, instanceID);
	}

	void OnPlayerAdded(const MasterPackets::PlayerAdded& added, const SystemAddress&) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		for (auto& world : ServerState::g_WorldInstances) {
			if (world.mapID == added.zoneID && world.instanceID == added.instanceID) {
				world.players++;
				break;
			}
		}
	}

	void OnPlayerRemoved(const MasterPackets::PlayerRemoved& removed, const SystemAddress&) {
		std::lock_guard lock(ServerState::g_StatusMutex);
		for (auto& world : ServerState::g_WorldInstances) {
			if (world.mapID == removed.zoneID && world.instanceID == removed.instanceID) {
				if (world.players > 0) world.players--;
				break;
			}
		}
	}

	void OnWorldShutDown(const MasterPackets::WorldShutDown& shutDown, const SystemAddress&) {
		const LWOMAPID zoneID = shutDown.zoneID;
		const LWOINSTANCEID instanceID = shutDown.instanceID;

		std::lock_guard lock(ServerState::g_StatusMutex);
		auto& instances = ServerState::g_WorldInstances;
		instances.erase(
			std::remove_if(instances.begin(), instances.end(),
				[zoneID, instanceID](const WorldInstanceInfo& w) {
					return w.mapID == zoneID && w.instanceID == instanceID;
				}),
			instances.end());

		LOG("World shutdown: zone %i instance %i", zoneID, instanceID);
	}

	// Packets from master
	const PacketDispatcher<MessageType::Master>& MasterHandlers() {
		static const auto handlers = [] {
			PacketDispatcher<MessageType::Master> handlers;
			using MessageType::Master;
			handlers.On<MasterPackets::ServerListResponse>(Master::SERVER_LIST_RESPONSE, OnServerList);
			handlers.On<MasterPackets::ServerInfo>(Master::SERVER_INFO, OnServerInfo);
			handlers.On<MasterPackets::WorldReadyInfo>(Master::WORLD_READY, OnWorldReady);
			handlers.On<MasterPackets::PlayerAdded>(Master::PLAYER_ADDED, OnPlayerAdded);
			handlers.On<MasterPackets::PlayerRemoved>(Master::PLAYER_REMOVED, OnPlayerRemoved);
			handlers.On<PlayerPositions>(Master::PLAYER_POSITIONS, [](const PlayerPositions& positions, const SystemAddress&) { LiveWorld::HandlePlayerPositions(positions); });
			handlers.On<MessageCaptureData>(Master::MESSAGE_CAPTURE_DATA, [](const MessageCaptureData& data, const SystemAddress&) { Inspector::HandleData(data); });
			handlers.On<DataChanged>(Master::DATA_CHANGED, [](const DataChanged& changed, const SystemAddress&) { BroadcastDataChanged(changed); });
			handlers.On<PlayerActionResult>(Master::PLAYER_ACTION_RESULT, [](const PlayerActionResult& result, const SystemAddress&) { PlayerActions::HandleResult(result); });
			handlers.On<MasterPackets::WorldShutDown>(Master::SHUTDOWN_RESPONSE, OnWorldShutDown);
			return handlers;
		}();
		return handlers;
	}

	void HandleMasterPacket(Packet* packet) {
		MasterHandlers().Dispatch(packet, ServiceType::MASTER);
	}
}

int main(int argc, char** argv) {
	Diagnostics::SetProduceMemoryDump(true);
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	uint32_t maxClients = 999;
	uint32_t ourPort = 2006;
	std::string ourIP = "127.0.0.1";

	// Read config
	Game::config = new dConfig("dashboardconfig.ini");

	// Setup logger
	Server::SetupLogger("DashboardServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	LOG("Starting Dashboard Server");

	// Load settings
	if (Game::config->GetValue("max_clients") != "") 
		maxClients = std::stoi(Game::config->GetValue("max_clients"));
	
	if (Game::config->GetValue("port") != "") 
		ourPort = std::atoi(Game::config->GetValue("port").c_str());
	
	if (Game::config->GetValue("listen_ip") != "") 
		ourIP = Game::config->GetValue("listen_ip");

	// Connect to CDClient database
	try {
		const std::string cdclientPath = BinaryPathFinder::GetBinaryDir() / "resServer/CDServer.sqlite";
		CDClientDatabase::Connect(cdclientPath);
	} catch (std::exception& ex) {
		LOG("Failed to connect to CDClient database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Connect to the database
	try {
		Database::Connect();
	} catch (std::exception& ex) {
		LOG("Failed to connect to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Settings edited on the dashboard (server_config table) are layered over the files from here on
	Game::config->SetDatabaseSync(ConfigSync::Sync);

	// Load locale translations
	std::string clientPath = Game::config->GetValue("client_location");
	if (!clientPath.empty()) {
		std::string localePath = clientPath + "/locale/locale.xml";
		Locale::LoadFromFile(localePath);
	}

	// Get master info from database
	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	std::string masterPassword;
	auto masterInfo = Database::Get()->GetMasterInfo();
	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}

	// Setup network server for communicating with Master. It needs its own UDP ports (net_port and the one after):
	// reusing the web port would take chat's master connection port (chat_server_port + 1 = 2006 by default).
	const auto netPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("net_port")).value_or(2010);
	g_Server = new dServer(
		masterIP,
		netPort,
		0,
		maxClients,
		false,
		false,
		Game::logger,
		masterIP,
		masterPort,
		ServiceType::DASHBOARD, // Connect as dashboard to master
		Game::config,
		&Game::lastSignal,
		masterPassword
	);
	Game::server = g_Server;

	// What the worker threads read from the CDClient and the settings, read now on this thread: workers never query
	// the CDClient, read settings or touch the network (RakNet and mongoose are the main thread's)
	{
		const auto start = std::chrono::steady_clock::now();
		ClientAssets::Preload();
		PreloadZoneData();
		Scenery::Preload();
		WorldView::Preload();
		ZoneNames();
		LOG("Read the client data for the 3D views in %lld ms", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));
	}
	Workers::Start();

	// Initialize web server
	if (!Game::web.Startup(ourIP, ourPort)) {
		LOG("Failed to start web server on %s:%d", ourIP.c_str(), ourPort);
		return EXIT_FAILURE;
	}

	if (!InitializeJWTSecret()) {
		LOG("Dashboard authentication could not be initialized; refusing to start");
		return EXIT_FAILURE;
	}

	Game::web.SetDefaultHeaders({
		"X-Content-Type-Options: nosniff",
		"X-Frame-Options: DENY",
		"Referrer-Policy: same-origin",
		"Cache-Control: no-store",
		// Pages use inline scripts and styles, so this mainly limits where code, connections and forms can go
		"Content-Security-Policy: default-src 'self'; "
			"script-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://cdn.datatables.net https://cdn.jsdelivr.net; "
			"style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://cdn.datatables.net; "
			"img-src 'self' data: blob:; font-src 'self' data: https://cdnjs.cloudflare.com; connect-src 'self'; "
			"object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'"
	});

	// WebSocket connections carry the session cookie; the level gates which topics they may receive
	// Also called again for open sockets every minute and when an account changes (BroadcastTableChanged("accounts"))
	Game::web.SetWSAuthCallback([](const std::string& token) -> std::optional<WSAuth> {
		const auto result = AuthTokenHandler::ValidateToken(token);
		if (!result.isValid) return std::nullopt;
		// Until required two-factor login is set up the session only reaches its own account page
		if (DashboardAuthService::NeedsTwoFactorSetup(result.accountId, result.gmLevel)) return WSAuth{ 0, result.accountId };
		return WSAuth{ result.gmLevel, result.accountId };
	});

	if (!Totp::LoadKey()) LOG("Two-factor login is unavailable: no usable key");
	EmailService::Initialize();
	Alerts::Initialize();
	Background::Initialize();
	RegisterEconomyTasks();
	RegisterMaintenanceTasks();
	RegisterBackupTask();
	RegisterReportViewTask();
	RegisterCharacterTasks();
	Inspector::Initialize();
	Scheduler::Initialize();

	// Register global middleware
	Game::web.AddGlobalMiddleware(std::make_shared<AuthMiddleware>());

	// Register routes in order: API, Static, Auth, WebSocket, Dashboard (dashboard MUST be last)
	RegisterAPIRoutes();
	RegisterStaticRoutes();
	RegisterAuthRoutes();
	RegisterAccountRoutes();
	RegisterClientAssetRoutes();
	RegisterMaintenanceRoutes();
	RegisterReportRoutes();
	RegisterSecurityRoutes();
	RegisterWebhookRoutes();
	RegisterEconomyJobRoutes();
	Scheduler::RegisterRoutes();
	RegisterBackupRoutes();
	RegisterReportViewRoutes();
	RegisterCharacterToolRoutes();
	RegisterCharacterRestoreRoutes();
	RegisterMissionToolRoutes();
	RegisterCharacterProgressRoutes();
	RegisterServerRoutes();
	RegisterLeaderboardRoutes();
	RequireAuthMiddleware::SetApiAccessCheck([](uint8_t gmLevel) { return Permissions::Allowed(gmLevel, "api_access"); });
	RequireAuthMiddleware::SetForbiddenPage([](const HTTPContext& context, HTTPReply& reply) {
		RouteUtils::RenderError(reply, context, eHTTPStatusCode::FORBIDDEN, "You don't have permission to open this page.");
	});
	Game::web.SetWSApiAccessCallback([](uint8_t gmLevel) { return Permissions::Allowed(gmLevel, "api_access"); });
	RegisterVanityRoutes();
	RegisterChatRoutes();
	RegisterStrikeRoutes();
	RegisterModerationToolRoutes();
	ModeratorHelper::RegisterRoutes();
	LiveWorld::RegisterRoutes();
	Inspector::RegisterRoutes();
	RegisterCDClientBrowserRoutes();
	RegisterSettingsRoutes();
	SettingsHistory::RegisterRoutes();
	Announcements::RegisterRoutes();
	EventsCalendar::RegisterRoutes();
	LiveEventRoutes::RegisterRoutes();
	ChallengeRoutes::RegisterRoutes();
	InstanceLoad::RegisterRoutes();
	WorldView::RegisterRoutes();
	Scenery::RegisterRoutes();
	PrometheusMetrics::RegisterRoutes();
	RegisterPublicRoutes();
	RegisterShowcaseRoutes();
	FeaturedProperties::RegisterRoutes();
	ContrabandRoutes::RegisterRoutes();
	UgcRoutes::RegisterRoutes();
	PropertyRentRoutes::RegisterRoutes();
	RegisterPasswordRecoveryRoutes();
	RegisterWSRoutes();
	RegisterDashboardRoutes(); // Must be last - catches all unmatched routes

	LOG("Dashboard Server started successfully on %s:%d", ourIP.c_str(), ourPort);
	LOG("Connected to Master Server at %s:%d", masterIP.c_str(), masterPort);

	// Main loop
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto lastBroadcast = lastTime;
	auto currentTime = lastTime;
	constexpr float deltaTime = 1.0f / 60.0f; // 60 FPS
	const float broadcastInterval = GeneralUtils::TryParse<float>(Game::config->GetValue("broadcast_interval")).value_or(2000.0f);

	while (!Game::ShouldShutdown()) {
		// The web server runs every pass, not just once per tick: a request takes several polls (accept, read,
		// reply), and waiting a tick between each capped the dashboard at a few dozen requests a second.
		// The poll's wait for network traffic is also this loop's sleep.
		Game::web.ReceiveRequests(2);

		currentTime = std::chrono::high_resolution_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
		const auto elapsedSinceBroadcast = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastBroadcast).count();

		if (elapsed >= 1000.0f / 60.0f) {
			Packet* packet = g_Server->ReceiveFromMaster();
			while (packet) {
				HandleMasterPacket(packet);
				g_Server->DeallocateMasterPacket(packet);
				packet = g_Server->ReceiveFromMaster();
			}

			// Only once the master link is up; sent earlier, the request is dropped and auth/chat look offline
			if (g_Server->GetIsConnectedToMaster() && std::chrono::steady_clock::now() >= g_NextServerListRequest) {
				MasterPackets::SendToMaster(MasterPackets::RequestServerList());
				g_NextServerListRequest = std::chrono::steady_clock::now() + SERVER_LIST_INTERVAL;
			}

			PlayerActions::Update();
			EmailService::Update();
			Alerts::Update();
			Background::Update();
			ModeratorHelper::Update();
			Scheduler::Update();
			ServerRoutes::Update();
			LiveWorld::Update();
			Inspector::Update();
			Announcements::Update();
			EventsCalendar::Update();
			LiveEventRoutes::Update();
			ChallengeRoutes::Update();
			InstanceLoad::Update();

			// Broadcast dashboard updates periodically
			if (elapsedSinceBroadcast >= broadcastInterval) {
				BroadcastDashboardUpdate();
				lastBroadcast = currentTime;
			}

			lastTime = currentTime;
		}

	}

	// Cleanup: the worker threads first (they answer deferred requests), then the web server's connections
	Workers::Stop();
	Game::web.Shutdown();
	Inspector::Shutdown();
	EmailService::Shutdown();
	ModeratorHelper::Shutdown();
	Background::Shutdown();
	Alerts::Shutdown();
	Database::Destroy("DashboardServer");
	delete g_Server;
	g_Server = nullptr;
	Game::server = nullptr;
	delete Game::logger;
	Game::logger = nullptr;
	delete Game::config;
	Game::config = nullptr;

	return EXIT_SUCCESS;
}


