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
#include "LiveEventRoutes.h"
#include "ChallengeRoutes.h"
#include "Inspector.h"
#include "CDClientBrowser.h"
#include "MessageCapture.h"
#include "PublicRoutes.h"
#include "Showcase.h"
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
#include "DashboardMessages.h"
#include "DataChanged.h"
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

	void HandleMasterPacket(Packet* packet) {
		if (packet->length < 4) return;
		if (static_cast<ServiceType>(packet->data[1]) != ServiceType::MASTER) return;

		switch (static_cast<MessageType::Master>(packet->data[3])) {
		case MessageType::Master::SERVER_LIST_RESPONSE: {
			CINSTREAM_SKIP_HEADER;

			uint8_t authOnline = 0;
			uint8_t chatOnline = 0;
			uint32_t instanceCount = 0;

			inStream.Read(authOnline);
			inStream.Read(chatOnline);
			inStream.Read(instanceCount);

			std::lock_guard lock(ServerState::g_StatusMutex);
			ServerState::g_AuthStatus.online = authOnline != 0;
			ServerState::g_AuthStatus.lastSeen = std::chrono::steady_clock::now();
			ServerState::g_ChatStatus.online = chatOnline != 0;
			ServerState::g_ChatStatus.lastSeen = std::chrono::steady_clock::now();

			ServerState::g_WorldInstances.clear();
			for (uint32_t i = 0; i < instanceCount; i++) {
				WorldInstanceInfo info;
				LUString ip;
				// Same types as MasterServer writes them (map and instance IDs are 16 bits)
				LWOMAPID mapID = 0;
				LWOINSTANCEID instanceID = 0;
				inStream.Read(mapID);
				inStream.Read(instanceID);
				info.mapID = mapID;
				info.instanceID = instanceID;
				inStream.Read(info.cloneID);
				inStream.Read(info.players);
				inStream.Read(ip);
				info.ip = ip.string;
				inStream.Read(info.port);
				uint8_t isPrivate = 0;
				inStream.Read(isPrivate);
				info.isPrivate = isPrivate != 0;
				info.zoneName = GetZoneDisplayName(info.mapID);
				AddPropertyDetails(info);
				ServerState::g_WorldInstances.push_back(info);
			}

			LOG_DEBUG("Received server list: auth=%s chat=%s worlds=%u",
				authOnline ? "online" : "offline",
				chatOnline ? "online" : "offline",
				instanceCount);
			break;
		}

		case MessageType::Master::SERVER_INFO: {
			CINSTREAM_SKIP_HEADER;

			uint32_t theirPort = 0;
			uint32_t theirZoneID = 0;
			uint32_t theirInstanceID = 0;
			ServiceType theirServerType;
			LUString theirIP;

			inStream.Read(theirPort);
			inStream.Read(theirZoneID);
			inStream.Read(theirInstanceID);
			inStream.Read(theirServerType);
			inStream.Read(theirIP);

			std::lock_guard lock(ServerState::g_StatusMutex);
			switch (theirServerType) {
			case ServiceType::AUTH:
				if (theirIP.string == "offline") {
					ServerState::g_AuthStatus.online = false;
				} else {
					ServerState::g_AuthStatus.online = true;
					ServerState::g_AuthStatus.lastSeen = std::chrono::steady_clock::now();
				}
				break;
			case ServiceType::CHAT:
				if (theirIP.string == "offline") {
					ServerState::g_ChatStatus.online = false;
				} else {
					ServerState::g_ChatStatus.online = true;
					ServerState::g_ChatStatus.lastSeen = std::chrono::steady_clock::now();
				}
				break;
			default:
				break;
			}
			break;
		}

		case MessageType::Master::WORLD_READY: {
			CINSTREAM_SKIP_HEADER;

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;
			LWOCLONEID cloneID;
			LUString ip;
			uint32_t port;
			uint8_t isPrivate;

			inStream.Read(zoneID);
			inStream.Read(instanceID);
			inStream.Read(cloneID);
			inStream.Read(ip);
			inStream.Read(port);
			inStream.Read(isPrivate);

			std::lock_guard lock(ServerState::g_StatusMutex);
			WorldInstanceInfo info;
			info.mapID = zoneID;
			info.instanceID = instanceID;
			info.cloneID = cloneID;
			info.players = 0;
			info.ip = ip.string;
			info.port = port;
			info.isPrivate = isPrivate != 0;
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
			break;
		}

		case MessageType::Master::PLAYER_ADDED: {
			CINSTREAM_SKIP_HEADER;

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;
			inStream.Read(zoneID);
			inStream.Read(instanceID);

			std::lock_guard lock(ServerState::g_StatusMutex);
			for (auto& world : ServerState::g_WorldInstances) {
				if (world.mapID == zoneID && world.instanceID == instanceID) {
					world.players++;
					break;
				}
			}
			break;
		}

		case MessageType::Master::PLAYER_REMOVED: {
			CINSTREAM_SKIP_HEADER;

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;
			inStream.Read(zoneID);
			inStream.Read(instanceID);

			std::lock_guard lock(ServerState::g_StatusMutex);
			for (auto& world : ServerState::g_WorldInstances) {
				if (world.mapID == zoneID && world.instanceID == instanceID) {
					if (world.players > 0) world.players--;
					break;
				}
			}
			break;
		}

		case MessageType::Master::PLAYER_POSITIONS: {
			CINSTREAM_SKIP_HEADER;
			PlayerPositions positions;
			if (positions.Deserialize(inStream)) LiveWorld::HandlePlayerPositions(positions);
			break;
		}

		case MessageType::Master::MESSAGE_CAPTURE_DATA: {
			CINSTREAM_SKIP_HEADER;
			MessageCaptureData data;
			if (data.Deserialize(inStream)) Inspector::HandleData(data);
			break;
		}

		case MessageType::Master::DATA_CHANGED: {
			CINSTREAM_SKIP_HEADER;
			DataChanged changed;
			if (changed.Deserialize(inStream)) BroadcastDataChanged(changed);
			break;
		}

		case MessageType::Master::PLAYER_ACTION_RESULT: {
			CINSTREAM_SKIP_HEADER;
			PlayerActionResult result;
			if (result.Deserialize(inStream)) PlayerActions::HandleResult(result);
			break;
		}

		case MessageType::Master::SHUTDOWN_RESPONSE: {
			CINSTREAM_SKIP_HEADER;

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;
			inStream.Read(zoneID);
			inStream.Read(instanceID);

			std::lock_guard lock(ServerState::g_StatusMutex);
			auto& instances = ServerState::g_WorldInstances;
			instances.erase(
				std::remove_if(instances.begin(), instances.end(),
					[zoneID, instanceID](const WorldInstanceInfo& w) {
						return w.mapID == zoneID && w.instanceID == instanceID;
					}),
				instances.end());

			LOG("World shutdown: zone %i instance %i", zoneID, instanceID);
			break;
		}

		default:
			break;
		}
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
				RakNet::BitStream bitStream;
				BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_SERVER_LIST);
				g_Server->SendToMaster(bitStream);
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

	// Cleanup: the conversion threads first (they answer deferred requests), then the web server's connections
	Scenery::Shutdown();
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


