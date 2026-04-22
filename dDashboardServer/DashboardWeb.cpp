#include "DashboardWeb.h"
#include "DashboardShared.h"

// Blueprint includes
#include "blueprints/AuthBlueprint.h"
#include "blueprints/ApiBlueprint.h"
#include "blueprints/PageBlueprint.h"
#include "blueprints/PlayKeysBlueprint.h"
#include "blueprints/CharactersBlueprint.h"
#include "blueprints/MailBlueprint.h"
#include "blueprints/BugReportsBlueprint.h"
#include "blueprints/ModerationBlueprint.h"

// Crow headers - must come before ASIO to avoid conflicts
#include "crow.h"
#include "crow/middlewares/session.h"

// thanks bill gates
#ifdef _WIN32
#undef min
#undef max
#endif

#include <memory>
#include <thread>
#include <chrono>
#include <iostream>

namespace DashboardWeb {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;

	static crow::App<crow::CookieParser, Session> g_App {
		Session{
			// cookie config: use "session" cookie name, 24h max_age
			crow::CookieParser::Cookie("session").max_age(24 * 60 * 60).path("/"),
			// session id length
			32,
			// storage backend (InMemoryStore)
			crow::InMemoryStore{}
		}
	};

	static std::future<void> g_ServerFuture;
	static bool g_Running = false;
	static bool g_Initialized = false;

	void SetupRoutes() {
		static bool setupCalled = false;
		if (setupCalled) {
			std::cerr << "WARNING: SetupRoutes() called multiple times!" << std::endl;
			return;
		}
		setupCalled = true;

		std::cerr << "Setting up dashboard routes..." << std::endl;

		// Set mustache template base directory
		crow::mustache::set_base("./templates");

		// Setup all blueprint routes
		try {
			std::cerr << "  - Setting up AuthBlueprint..." << std::endl;
			AuthBlueprint::Setup(g_App);

			std::cerr << "  - Setting up ApiBlueprint..." << std::endl;
			ApiBlueprint::Setup(g_App);

			std::cerr << "  - Setting up PageBlueprint..." << std::endl;
			PageBlueprint::Setup(g_App);

			std::cerr << "  - Setting up PlayKeysBlueprint..." << std::endl;
			PlayKeysBlueprint::Setup(g_App);

			std::cerr << "  - Setting up CharactersBlueprint..." << std::endl;
			CharactersBlueprint::Setup(g_App);

			std::cerr << "  - Setting up MailBlueprint..." << std::endl;
			MailBlueprint::Setup(g_App);

			std::cerr << "  - Setting up BugReportsBlueprint..." << std::endl;
			BugReportsBlueprint::Setup(g_App);

			std::cerr << "  - Setting up ModerationBlueprint..." << std::endl;
			ModerationBlueprint::Setup(g_App);

			std::cerr << "All routes set up successfully!" << std::endl;
		} catch (const std::exception& e) {
			// Print to stderr since LOG might not be available
			std::cerr << "Error setting up routes: " << e.what() << std::endl;
			throw;
		}
	}

	void Initialize(uint32_t port) {
		// Only allow initialization once per process lifetime
		// Crow apps cannot be restarted once stopped
		if (g_Initialized) {
			std::cerr << "Dashboard web server already initialized. Cannot reinitialize." << std::endl;
			return;
		}

		try {
			// Setup routes (only happens once)
			SetupRoutes();

			// Configure Crow app
			g_App.loglevel(crow::LogLevel::Info); // Changed to Info to see startup messages

			// Start the server in a separate thread
			g_ServerFuture = std::async(std::launch::async, [port]() {
				try {
					g_App.port(port).multithreaded().run();
				} catch (const std::exception& e) {
					std::cerr << "Error running Crow server: " << e.what() << std::endl;
				}
			});

			g_Running = true;
			g_Initialized = true;

			// Give the server a moment to start
			std::this_thread::sleep_for(std::chrono::milliseconds(500));

		} catch (const std::exception& e) {
			std::cerr << "Error initializing dashboard web server: " << e.what() << std::endl;
			throw;
		}
	}

	void Update() {
		// Crow runs in its own thread, nothing to update here
	}

	void Stop() {
		if (!g_Running) {
			return;
		}

		g_App.stop();

		// Wait for the server thread to finish (with timeout)
		if (g_ServerFuture.valid()) {
			auto status = g_ServerFuture.wait_for(std::chrono::seconds(5));
			if (status == std::future_status::timeout) {
				std::cerr << "Warning: Dashboard web server did not stop gracefully" << std::endl;
			}
		}

		g_Running = false;
	}

} // namespace DashboardWeb
