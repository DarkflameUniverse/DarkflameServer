#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "ServerState.h"

namespace ServerState {
	ServerStatus g_AuthStatus{};
	ServerStatus g_ChatStatus{};
	std::vector<WorldInstanceInfo> g_WorldInstances{};
	std::mutex g_StatusMutex{};
}

class ServerStateTest : public ::testing::Test {
protected:
	void SetUp() override {
		std::lock_guard lock(ServerState::g_StatusMutex);
		ServerState::g_AuthStatus = {};
		ServerState::g_ChatStatus = {};
		ServerState::g_WorldInstances.clear();
	}
};

TEST_F(ServerStateTest, DefaultStateAllOffline) {
	auto json = ServerState::GetServerStateJson();

	EXPECT_FALSE(json["auth"]["online"].get<bool>());
	EXPECT_FALSE(json["chat"]["online"].get<bool>());
	EXPECT_TRUE(json["worlds"].empty());
	EXPECT_EQ(json["stats"]["onlinePlayers"].get<uint32_t>(), 0);
}

TEST_F(ServerStateTest, AuthOnlineReflected) {
	{
		std::lock_guard lock(ServerState::g_StatusMutex);
		ServerState::g_AuthStatus.online = true;
	}

	auto json = ServerState::GetServerStateJson();
	EXPECT_TRUE(json["auth"]["online"].get<bool>());
	EXPECT_FALSE(json["chat"]["online"].get<bool>());
}

TEST_F(ServerStateTest, ChatOnlineReflected) {
	{
		std::lock_guard lock(ServerState::g_StatusMutex);
		ServerState::g_ChatStatus.online = true;
	}

	auto json = ServerState::GetServerStateJson();
	EXPECT_TRUE(json["chat"]["online"].get<bool>());
}

TEST_F(ServerStateTest, WorldInstancesSerializedCorrectly) {
	{
		std::lock_guard lock(ServerState::g_StatusMutex);
		WorldInstanceInfo w1;
		w1.mapID = 1000;
		w1.instanceID = 1;
		w1.cloneID = 0;
		w1.players = 5;
		w1.isPrivate = false;

		WorldInstanceInfo w2;
		w2.mapID = 1100;
		w2.instanceID = 2;
		w2.cloneID = 0;
		w2.players = 3;
		w2.isPrivate = true;

		ServerState::g_WorldInstances.push_back(w1);
		ServerState::g_WorldInstances.push_back(w2);
	}

	auto json = ServerState::GetServerStateJson();

	ASSERT_EQ(json["worlds"].size(), 2);

	EXPECT_EQ(json["worlds"][0]["mapID"].get<uint32_t>(), 1000);
	EXPECT_EQ(json["worlds"][0]["instanceID"].get<uint32_t>(), 1);
	EXPECT_EQ(json["worlds"][0]["players"].get<uint32_t>(), 5);
	EXPECT_FALSE(json["worlds"][0]["isPrivate"].get<bool>());

	EXPECT_EQ(json["worlds"][1]["mapID"].get<uint32_t>(), 1100);
	EXPECT_EQ(json["worlds"][1]["instanceID"].get<uint32_t>(), 2);
	EXPECT_EQ(json["worlds"][1]["players"].get<uint32_t>(), 3);
	EXPECT_TRUE(json["worlds"][1]["isPrivate"].get<bool>());
}

TEST_F(ServerStateTest, OnlinePlayersCountSumsAllWorlds) {
	{
		std::lock_guard lock(ServerState::g_StatusMutex);
		WorldInstanceInfo w1;
		w1.mapID = 1000;
		w1.instanceID = 1;
		w1.players = 10;
		ServerState::g_WorldInstances.push_back(w1);

		WorldInstanceInfo w2;
		w2.mapID = 1100;
		w2.instanceID = 2;
		w2.players = 7;
		ServerState::g_WorldInstances.push_back(w2);

		WorldInstanceInfo w3;
		w3.mapID = 1200;
		w3.instanceID = 3;
		w3.players = 0;
		ServerState::g_WorldInstances.push_back(w3);
	}

	auto json = ServerState::GetServerStateJson();
	EXPECT_EQ(json["stats"]["onlinePlayers"].get<uint32_t>(), 17);
}

TEST_F(ServerStateTest, EmptyWorldsGivesZeroPlayers) {
	auto json = ServerState::GetServerStateJson();
	EXPECT_EQ(json["stats"]["onlinePlayers"].get<uint32_t>(), 0);
	EXPECT_EQ(json["worlds"].size(), 0);
}

TEST_F(ServerStateTest, ConcurrentAccessDoesNotCrash) {
	auto writer = [&]() {
		for (int i = 0; i < 100; i++) {
			std::lock_guard lock(ServerState::g_StatusMutex);
			WorldInstanceInfo w;
			w.mapID = 1000 + i;
			w.instanceID = i;
			w.players = i;
			ServerState::g_WorldInstances.push_back(w);
		}
	};

	auto reader = [&]() {
		for (int i = 0; i < 100; i++) {
			auto json = ServerState::GetServerStateJson();
			EXPECT_TRUE(json.contains("auth"));
			EXPECT_TRUE(json.contains("worlds"));
			EXPECT_TRUE(json.contains("stats"));
		}
	};

	std::thread t1(writer);
	std::thread t2(reader);
	std::thread t3(reader);

	t1.join();
	t2.join();
	t3.join();
}

TEST_F(ServerStateTest, WorldInstanceInfoDefaults) {
	WorldInstanceInfo info;
	EXPECT_EQ(info.mapID, 0);
	EXPECT_EQ(info.instanceID, 0);
	EXPECT_EQ(info.cloneID, 0);
	EXPECT_EQ(info.players, 0);
	EXPECT_EQ(info.ip, "");
	EXPECT_EQ(info.port, 0);
	EXPECT_FALSE(info.isPrivate);
}

TEST_F(ServerStateTest, ServerStatusDefaults) {
	ServerStatus status;
	EXPECT_FALSE(status.online);
	EXPECT_EQ(status.players, 0);
	EXPECT_EQ(status.version, "");
}
