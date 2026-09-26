#include <gtest/gtest.h>
#include <filesystem>
#include "Game.h"
#include "Logger.h"

class dConfig;

namespace Game {
	Logger* logger = nullptr;
	dConfig* config = nullptr; // code under test treats a missing config as all defaults
}

// Code under test logs through Game::logger, so give it a real logger writing to a temp file
class WebTestEnvironment : public ::testing::Environment {
public:
	void SetUp() override {
		const auto path = std::filesystem::temp_directory_path() / "dWebTests.log";
		Game::logger = new Logger(path.string(), false, false);
	}

	void TearDown() override {
		delete Game::logger;
		Game::logger = nullptr;
	}
};

static const auto* const g_Environment = ::testing::AddGlobalTestEnvironment(new WebTestEnvironment());
