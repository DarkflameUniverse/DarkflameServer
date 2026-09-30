// The capture viewer decodes every packet the server sends or reads: every LU message ID has a struct (or is on the
// list below of IDs this server never sends or handles), every game message struct reads, and samples of each family
// come back with the values they were written with.
#include <gtest/gtest.h>

#include <set>
#include <string>

#include "AuthPackets.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "CommonPackets.h"
#include "EffectsMessages.h"
#include "GameMessageDecoder.h"
#include "GameMessageHandler.h"
#include "MasterPackets.h"
#include "MessageType/Auth.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Master.h"
#include "MessageType/Server.h"
#include "MessageType/World.h"
#include "PacketDecoder.h"
#include "ServiceType.h"
#include "SkillMessages.h"
#include "TradeMessages.h"
#include "WorldPackets.h"
#include "magic_enum.hpp"

namespace {
	using json = nlohmann::json;

	std::string Bytes(const RakNet::BitStream& stream) { return std::string(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed()); }

	template<typename T>
	PacketDecoder::Decoded Written(const T& packet, bool fromClient) {
		RakNet::BitStream stream;
		packet.WritePacket(stream);
		return PacketDecoder::Decode(Bytes(stream), fromClient);
	}

	// IDs this server never sends and never handles: no struct, so the viewer shows their name and bytes
	const std::set<std::string> NOT_USED{
		"AUTH LOGOUT_REQUEST", "AUTH CREATE_NEW_ACCOUNT_REQUEST", "AUTH LEGOINTERFACE_AUTH_RESPONSE", "AUTH SESSIONKEY_RECEIVED_CONFIRM",
		"CHAT USER_CHANNEL_CHAT_MESSAGE", "CHAT WORLD_DISCONNECT_REQUEST", "CHAT WORLD_PROXIMITY_RESPONSE", "CHAT WORLD_PARCEL_RESPONSE",
		"CHAT TEAM_MISSED_INVITE_CHECK", "CHAT BLUEPRINT_MODERATED", "CHAT BLUEPRINT_MODEL_READY", "CHAT PROPERTY_READY_FOR_APPROVAL",
		"CHAT PROPERTY_MODERATION_CHANGED", "CHAT PROPERTY_BUILDMODE_CHANGED", "CHAT PROPERTY_BUILDMODE_CHANGED_REPORT",
		"CHAT WORLD_INSTANCE_LOCATION_REQUEST", "CHAT REPUTATION_UPDATE", "CHAT SEND_CANNED_TEXT", "CHAT CHARACTER_NAME_CHANGE_REQUEST",
		"CHAT CSR_REQUEST", "CHAT CSR_REPLY", "CHAT GM_KICK", "CHAT ACTIVITY_UPDATE", "CHAT GET_ZONE_POPULATIONS",
		"CHAT UGCMANIFEST_REPORT_MISSING_FILE", "CHAT UGCMANIFEST_REPORT_DONE_FILE", "CHAT UGCMANIFEST_REPORT_DONE_BLUEPRINT",
		"CHAT UGCC_REQUEST", "CHAT WORLD_PLAYERS_PET_MODERATED_ACKNOWLEDGE", "CHAT GM_CLOSE_PRIVATE_CHAT_WINDOW", "CHAT PLAYER_READY",
		"CHAT GET_DONATION_TOTAL", "CHAT UPDATE_DONATION", "CHAT PRG_CSR_COMMAND", "CHAT HEARTBEAT_REQUEST_FROM_WORLD",
		"CHAT UPDATE_FREE_TRIAL_STATUS",
		"CLIENT LOGOUT_RESPONSE", "CLIENT CREATE_OBJECT", "CLIENT CREATE_CHARACTER_EXTENDED", "CLIENT CHAT_CONNECT_RESPONSE",
		"CLIENT AUTH_ACCOUNT_CREATE_RESPONSE", "CLIENT CONNECT_CHAT", "CLIENT IMPENDING_RELOAD_NOTIFY", "CLIENT SLASH_PUSH_MAP_RESPONSE",
		"CLIENT SLASH_PULL_MAP_RESPONSE", "CLIENT SLASH_LOCK_MAP_RESPONSE", "CLIENT BLUEPRINT_LUP_SAVE_RESPONSE",
		"CLIENT BLUEPRINT_GET_ALL_DATA_RESPONSE", "CLIENT MODEL_INSTANTIATE_RESPONSE", "CLIENT GUILD_GET_STATUS_RESPONSE",
		"CLIENT GUILD_RANK_CHANGE", "CLIENT GUILD_STATUS", "CLIENT DB_PROXY_RESULT", "CLIENT UPDATE_CHARACTER_NAME",
		"CLIENT SET_NETWORK_SIMULATOR", "CLIENT INVALID_CHAT_MESSAGE", "CLIENT IN_LOGIN_QUEUE", "CLIENT GM_CLOSE_TARGET_CHAT_WINDOW",
		"CLIENT GENERAL_TEXT_FOR_LOCALIZATION", "CLIENT UPDATE_FREE_TRIAL_STATUS",
		"MASTER SHUTDOWN_IMMEDIATE", "AUTH RUNTIME_CONFIG", "CLIENT UGC_DOWNLOAD_FAILED",
		"WORLD HAPPY_FLOWER_MODE_NOTIFY", "WORLD SLASH_RELOAD_MAP", "WORLD SLASH_PUSH_MAP_REQUEST", "WORLD SLASH_PUSH_MAP",
		"WORLD SLASH_PULL_MAP", "WORLD LOCK_MAP_REQUEST", "WORLD HTTP_MONITOR_INFO_REQUEST", "WORLD SLASH_DEBUG_SCRIPTS",
		"WORLD MODELS_CLEAR", "WORLD EXHIBIT_INSERT_MODEL", "WORLD WORD_CHECK", "WORLD GET_PLAYERS_IN_ZONE",
		"WORLD BLUEPRINT_GET_ALL_DATA_REQUEST", "WORLD CANCEL_MAP_QUEUE", "WORLD FAKE_PRG_CSR_MESSAGE",
		"WORLD REQUEST_FREE_TRIAL_REFRESH", "WORLD GM_SET_FREE_TRIAL_STATUS",
	};

	// Decoded some other way: game messages (GameMessageDecoder) and mail (a sub-header of its own, read by the game)
	const std::set<std::string> ELSEWHERE{ "CLIENT GAME_MSG", "CLIENT MAIL" };

	template<typename E>
	void ExpectEveryIdDecodes(ServiceType service, std::set<std::string>& missing) {
		for (const auto id : magic_enum::enum_values<E>()) {
			const auto name = std::string(magic_enum::enum_name(service)) + " " + std::string(magic_enum::enum_name(id));
			if (PacketDecoder::HasFields(service, static_cast<uint32_t>(id))) {
				EXPECT_FALSE(NOT_USED.contains(name)) << name << " has a struct now: take it off the list";
				continue;
			}
			if (!NOT_USED.contains(name) && !ELSEWHERE.contains(name)) missing.insert(name);
		}
	}
}

TEST(PacketDecoderCoverageTest, EveryMessageTypeHasAStruct) {
	std::set<std::string> missing;
	ExpectEveryIdDecodes<MessageType::Server>(ServiceType::COMMON, missing);
	ExpectEveryIdDecodes<MessageType::Auth>(ServiceType::AUTH, missing);
	ExpectEveryIdDecodes<MessageType::Chat>(ServiceType::CHAT, missing);
	ExpectEveryIdDecodes<MessageType::Client>(ServiceType::CLIENT, missing);
	ExpectEveryIdDecodes<MessageType::Master>(ServiceType::MASTER, missing);
	ExpectEveryIdDecodes<MessageType::World>(ServiceType::WORLD, missing);
	std::string list;
	for (const auto& name : missing) list += name + "\n";
	EXPECT_TRUE(missing.empty()) << "No struct decodes these (add one, or list them as not used):\n" << list;
}

// Every game message the server has a struct for reads, and so does every message it handles from a client
TEST(PacketDecoderCoverageTest, EveryGameMessageStructReads) {
	const auto decodable = GameMessageDecoder::Decodable();
	EXPECT_GT(decodable.size(), 290u);
	for (const auto id : magic_enum::enum_values<MessageType::Game>()) {
		if (!GameMessageHandler::CreateReceived(id)) continue;
		EXPECT_TRUE(GameMessageDecoder::CanDecode(id, true)) << magic_enum::enum_name(id);
	}
	for (const auto id : decodable) {
		EXPECT_TRUE(GameMessageDecoder::CanDecode(id, true) && GameMessageDecoder::CanDecode(id, false)) << magic_enum::enum_name(id);
	}
}

TEST(PacketDecoderCoverageTest, AuthLoginRequestNeverShowsThePassword) {
	AuthPackets::LoginRequest login;
	login.username.string = u"tester";
	login.password.string = u"hunter2";
	login.numberOfProcessors = 8;
	const auto decoded = Written(login, true);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["username"], "tester");
	EXPECT_EQ((*decoded.fields)["numberOfProcessors"], 8);
	EXPECT_FALSE(decoded.fields->contains("password"));
	EXPECT_EQ(decoded.fields->dump().find("hunter2"), std::string::npos);
}

TEST(PacketDecoderCoverageTest, SamplesOfEachFamilyReadBack) {
	CommonPackets::ServerVersionConfirm version;
	version.netVersion = 171022;
	auto decoded = Written(version, false);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["netVersion"], 171022);

	ChatPackets::GMMute mute;
	mute.playerID = 1152921504606846999LL;
	mute.expire = 60;
	decoded = Written(mute, false);
	ASSERT_TRUE(decoded.fields) << decoded.name;
	EXPECT_EQ((*decoded.fields)["playerID"], "1152921504606846999");
	EXPECT_EQ((*decoded.fields)["expire"], "60");

	MasterPackets::PlayerAdded added;
	added.zoneID = 1200;
	added.instanceID = 3;
	decoded = Written(added, false);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["zoneID"], 1200);
	EXPECT_EQ((*decoded.fields)["instanceID"], 3);

	ClientPackets::LoadStaticZone zone;
	zone.mapID = 1100;
	zone.mapChecksum = 0x49525511;
	zone.playerPosition = NiPoint3(1.0f, 2.0f, 3.0f);
	decoded = Written(zone, false);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["mapID"], 1100);
	EXPECT_EQ((*decoded.fields)["mapChecksum"], 0x49525511);
	EXPECT_EQ((*decoded.fields)["playerPosition"], json::array({ 1.0f, 2.0f, 3.0f }));

	WorldPackets::LevelLoadComplete loaded;
	loaded.mapID = 1100;
	decoded = Written(loaded, true);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["mapID"], 1100);
}

// Game messages the other way round from the ones the server reads: what it sends to clients
TEST(PacketDecoderCoverageTest, SentGameMessagesReadBack) {
	GameMessages::DisplayTooltip tooltip;
	tooltip.show = true;
	tooltip.time = 5000;
	tooltip.id = u"tip";
	tooltip.text = u"Press E";
	tooltip.localizeParams.Insert(u"count", 3);
	RakNet::BitStream payload;
	tooltip.Serialize(payload);
	const auto fields = GameMessageDecoder::Decode(MessageType::Game::DISPLAY_TOOLTIP, false, payload);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["show"], true);
	EXPECT_EQ((*fields)["time"], 5000);
	EXPECT_EQ((*fields)["text"], "Press E");
	EXPECT_EQ((*fields)["localizeParams"], json::array({ "count=1:3" }));

	GameMessages::ServerTradeUpdate update;
	GameMessages::TradeItemEntry item;
	item.key = 5;
	item.itemID = 5;
	item.templateID = 1727;
	item.count = 2u;
	update.inventoryMap.push_back(item);
	RakNet::BitStream tradePayload;
	update.Serialize(tradePayload);
	const auto trade = GameMessageDecoder::Decode(MessageType::Game::SERVER_TRADE_UPDATE, false, tradePayload);
	ASSERT_TRUE(trade);
	ASSERT_EQ((*trade)["inventoryMap"].size(), 1u);
	EXPECT_EQ((*trade)["inventoryMap"][0]["templateID"], 1727);
	EXPECT_EQ((*trade)["inventoryMap"][0]["count"], 2);
}
