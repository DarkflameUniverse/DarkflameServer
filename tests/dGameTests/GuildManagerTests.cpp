// The chat server's guild rules (dChatServer/GuildManager, docs/Guilds.md) against an in-memory IGuilds, with every
// packet it sends captured.
#include "GuildManager.h"

#include "ChatPackets.h"
#include "ClientPackets.h"
#include "GameDependencies.h"

#include <algorithm>
#include <map>

#include <gtest/gtest.h>

namespace {
	// IGuilds in memory, ordered the way the SQL implementations order
	class FakeGuilds : public IGuilds {
	public:
		std::map<int64_t, Guild> guilds;
		std::map<LWOOBJID, Member> members;
		std::map<LWOOBJID, Invite> invites;
		std::vector<Event> events;
		std::map<LWOOBJID, std::string> characterNames;
		int64_t nextId = 1;

		static std::string Lower(std::string text) {
			std::transform(text.begin(), text.end(), text.begin(), ::tolower);
			return text;
		}

		int64_t InsertGuild(const Guild& guild) override {
			if (GetGuildByName(guild.name)) throw std::runtime_error("duplicate name");
			auto copy = guild;
			copy.id = nextId++;
			guilds[copy.id] = copy;
			return copy.id;
		}
		std::optional<Guild> GetGuild(int64_t guildId) override {
			const auto it = guilds.find(guildId);
			return it == guilds.end() ? std::nullopt : std::optional<Guild>(it->second);
		}
		std::optional<Guild> GetGuildByName(const std::string& name) override {
			for (const auto& [id, guild] : guilds) if (Lower(guild.name) == Lower(name)) return guild;
			return std::nullopt;
		}
		void SetGuildName(int64_t guildId, const std::string& name, int32_t nameStatus) override {
			guilds[guildId].name = name;
			guilds[guildId].nameStatus = nameStatus;
		}
		void DeleteGuild(int64_t guildId) override {
			guilds.erase(guildId);
			std::erase_if(members, [&](const auto& entry) { return entry.second.guildId == guildId; });
			std::erase_if(invites, [&](const auto& entry) { return entry.second.guildId == guildId; });
		}
		void AddGuildMember(const Member& member) override {
			auto copy = member;
			copy.name = characterNames[member.characterId];
			members[member.characterId] = copy;
		}
		void RemoveGuildMember(LWOOBJID characterId) override { members.erase(characterId); }
		void SetGuildMemberRank(LWOOBJID characterId, uint8_t rank) override { members[characterId].rank = rank; }
		std::optional<Member> GetGuildMember(LWOOBJID characterId) override {
			const auto it = members.find(characterId);
			return it == members.end() ? std::nullopt : std::optional<Member>(it->second);
		}
		std::vector<Member> GetGuildMembers(int64_t guildId) override {
			std::vector<Member> result;
			for (const auto& [id, member] : members) if (member.guildId == guildId) result.push_back(member);
			std::sort(result.begin(), result.end(), [](const Member& a, const Member& b) {
				return std::tie(a.rank, a.joinedAt, a.characterId) < std::tie(b.rank, b.joinedAt, b.characterId);
			});
			return result;
		}
		void SetGuildInvite(const Invite& invite) override { invites[invite.characterId] = invite; }
		std::optional<Invite> GetGuildInvite(LWOOBJID characterId) override {
			const auto it = invites.find(characterId);
			return it == invites.end() ? std::nullopt : std::optional<Invite>(it->second);
		}
		void DeleteGuildInvite(LWOOBJID characterId) override { invites.erase(characterId); }
		uint64_t InsertGuildEvent(const Event& event) override {
			events.push_back(event);
			events.back().id = events.size();
			return events.size();
		}
		std::vector<Event> GetGuildEvents(int64_t guildId, uint32_t limit) override { return events; }
		GuildPage GetGuildPage(uint32_t, uint32_t, const std::string&, bool) override { return {}; }
	};

	struct Sent {
		LWOOBJID to{};
		bool toWorld{};
		uint32_t id{};
		std::vector<uint8_t> bytes;
	};

	constexpr LWOOBJID ALICE = 0x1000000000000001LL;
	constexpr LWOOBJID BOB = 0x1000000000000002LL;
	constexpr LWOOBJID CAROL = 0x1000000000000003LL;
	constexpr LWOOBJID DAVE = 0x1000000000000004LL;
	constexpr LWOOBJID ERIN = 0x1000000000000005LL; // exists, never online
}

class GuildManagerTests : public GameDependenciesTest {
protected:
	FakeGuilds db;
	std::map<LWOOBJID, GuildManager::OnlinePlayer> online;
	std::vector<Sent> sent;
	std::vector<std::pair<LWOOBJID, std::string>> notes;
	int64_t now = 1700000000;
	GuildManager::NameCheck nameCheck = GuildManager::NameCheck::APPROVED;
	std::unique_ptr<GuildManager> guilds;

	void SetUp() override {
		SetUpDependencies();
		const std::map<LWOOBJID, std::string> names{ { ALICE, "Alice" }, { BOB, "Bob" }, { CAROL, "Carol" }, { DAVE, "Dave" }, { ERIN, "Erin" } };
		db.characterNames = names;
		for (const auto id : { ALICE, BOB, CAROL, DAVE }) online[id] = { id, names.at(id), LWOZONEID(1200, 1, 0) };

		GuildManager::Hooks hooks;
		hooks.findOnline = [this](LWOOBJID id) -> std::optional<GuildManager::OnlinePlayer> {
			const auto it = online.find(id);
			return it == online.end() ? std::nullopt : std::optional<GuildManager::OnlinePlayer>(it->second);
		};
		hooks.findOnlineByName = [this](const std::string& name) -> std::optional<GuildManager::OnlinePlayer> {
			for (const auto& [id, player] : online) if (FakeGuilds::Lower(player.name) == FakeGuilds::Lower(name)) return player;
			return std::nullopt;
		};
		hooks.characterExists = [this](const std::string& name) {
			for (const auto& [id, known] : db.characterNames) if (FakeGuilds::Lower(known) == FakeGuilds::Lower(name)) return true;
			return false;
		};
		hooks.sendToPlayer = [this](LWOOBJID to, const LUBitStream& msg) { Record(to, false, msg); };
		hooks.sendToWorldOf = [this](LWOOBJID to, const LUBitStream& msg) { Record(to, true, msg); };
		hooks.notify = [this](LWOOBJID to, const std::string& text) { notes.emplace_back(to, text); };
		hooks.checkName = [this](const std::string&) { return nameCheck; };
		hooks.now = [this] { return now; };
		guilds = std::make_unique<GuildManager>(db, hooks, GuildManager::Settings{ 4, 600 });
	}
	void TearDown() override { TearDownDependencies(); }

	void Record(LWOOBJID to, bool toWorld, const LUBitStream& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		sent.push_back({ to, toWorld, msg.internalPacketID, { bitStream.GetData(), bitStream.GetData() + bitStream.GetNumberOfBytesUsed() } });
	}

	// The packets of type T sent to `to`, read back
	template<typename T>
	std::vector<T> To(LWOOBJID to) {
		std::vector<T> result;
		const T probe;
		for (const auto& packet : sent) {
			if (packet.to != to || packet.id != probe.internalPacketID) continue;
			const bool worldPacket = probe.connectionType == ServiceType::CHAT;
			if (packet.toWorld != worldPacket) continue;
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
			LUBitStream header;
			EXPECT_TRUE(header.ReadHeader(bitStream));
			T msg;
			EXPECT_TRUE(msg.Deserialize(bitStream));
			result.push_back(msg);
		}
		return result;
	}

	// Alice leads "Brick Builders"; each name in `joining` is invited by Alice and accepts
	int64_t MakeGuild(std::initializer_list<LWOOBJID> joining = {}) {
		guilds->Create(ALICE, u"Brick Builders");
		const auto guild = db.GetGuildByName("Brick Builders");
		EXPECT_TRUE(guild);
		for (const auto id : joining) {
			guilds->Invite(ALICE, online.at(id).name);
			now++;
			guilds->AnswerInvite(id, false);
		}
		sent.clear();
		notes.clear();
		return guild ? guild->id : 0;
	}
};

TEST_F(GuildManagerTests, NameRules) {
	EXPECT_TRUE(GuildManager::IsValidName(u"Brick Builders"));
	EXPECT_TRUE(GuildManager::IsValidName(u"O'Brien's-Crew.2"));
	EXPECT_TRUE(GuildManager::IsValidName(std::u16string(30, u'a')));
	EXPECT_FALSE(GuildManager::IsValidName(std::u16string(31, u'a')));
	EXPECT_FALSE(GuildManager::IsValidName(u"ab"));
	EXPECT_FALSE(GuildManager::IsValidName(u" Brick"));
	EXPECT_FALSE(GuildManager::IsValidName(u"Brick "));
	EXPECT_FALSE(GuildManager::IsValidName(u"Brick  Builders"));
	EXPECT_FALSE(GuildManager::IsValidName(u"Brick_Builders"));
	EXPECT_FALSE(GuildManager::IsValidName(u"Café"));
}

TEST_F(GuildManagerTests, RankRules) {
	using enum eGuildRank;
	EXPECT_TRUE(GuildManager::CanInvite(LEADER));
	EXPECT_TRUE(GuildManager::CanInvite(OFFICER));
	EXPECT_FALSE(GuildManager::CanInvite(VETERAN));
	EXPECT_FALSE(GuildManager::CanInvite(RECRUIT));
	EXPECT_TRUE(GuildManager::CanKick(LEADER, OFFICER));
	EXPECT_FALSE(GuildManager::CanKick(OFFICER, OFFICER));
	EXPECT_TRUE(GuildManager::CanKick(OFFICER, RECRUIT));
	EXPECT_FALSE(GuildManager::CanKick(VETERAN, RECRUIT));
	EXPECT_TRUE(GuildManager::CanSetRank(LEADER, RECRUIT, OFFICER));
	EXPECT_TRUE(GuildManager::CanSetRank(LEADER, OFFICER, LEADER));
	EXPECT_FALSE(GuildManager::CanSetRank(OFFICER, RECRUIT, OFFICER));
	EXPECT_TRUE(GuildManager::CanSetRank(OFFICER, RECRUIT, VETERAN));
	EXPECT_FALSE(GuildManager::CanSetRank(OFFICER, OFFICER, VETERAN));
	EXPECT_FALSE(GuildManager::CanSetRank(OFFICER, RECRUIT, LEADER));
	EXPECT_FALSE(GuildManager::CanSetRank(LEADER, RECRUIT, NONE));
}

TEST_F(GuildManagerTests, Create) {
	guilds->Create(ALICE, u"  Brick Builders ");
	const auto responses = To<ClientPackets::GuildCreateResponse>(ALICE);
	ASSERT_EQ(responses.size(), 1u);
	EXPECT_EQ(responses[0].result, eGuildCreateResponse::CREATED);
	EXPECT_EQ(responses[0].guildName, "Brick Builders");
	const auto guild = db.GetGuildByName("brick builders");
	ASSERT_TRUE(guild);
	EXPECT_EQ(responses[0].guildID, guild->id);
	EXPECT_EQ(guild->nameStatus, IGuilds::NAME_APPROVED);
	EXPECT_EQ(db.GetGuildMember(ALICE)->rank, static_cast<uint8_t>(eGuildRank::LEADER));
	// Alice's world puts the guild in her character component
	const auto status = To<ChatPackets::GuildStatus>(ALICE);
	ASSERT_EQ(status.size(), 1u);
	EXPECT_EQ(status[0].guildID, guild->id);
	EXPECT_EQ(status[0].guildName.GetAsString(), "Brick Builders");
	ASSERT_EQ(db.events.size(), 1u);
	EXPECT_EQ(db.events[0].kind, "created");
}

TEST_F(GuildManagerTests, CreateRefusals) {
	const auto result = [&](LWOOBJID player) {
		const auto responses = To<ClientPackets::GuildCreateResponse>(player);
		sent.clear();
		return responses.empty() ? eGuildCreateResponse::FAILED : responses.back().result;
	};
	guilds->Create(ALICE, u"x");
	EXPECT_EQ(result(ALICE), eGuildCreateResponse::BAD_NAME);
	nameCheck = GuildManager::NameCheck::DENIED;
	guilds->Create(ALICE, u"Rude Words");
	EXPECT_EQ(result(ALICE), eGuildCreateResponse::BAD_NAME);
	nameCheck = GuildManager::NameCheck::APPROVED;
	guilds->Create(ALICE, u"Brick Builders");
	EXPECT_EQ(result(ALICE), eGuildCreateResponse::CREATED);
	guilds->Create(BOB, u"BRICK builders");
	EXPECT_EQ(result(BOB), eGuildCreateResponse::EXISTS);
	// Already in a guild
	guilds->Create(ALICE, u"Second Guild");
	EXPECT_EQ(result(ALICE), eGuildCreateResponse::FAILED);
	EXPECT_EQ(db.guilds.size(), 1u);
}

TEST_F(GuildManagerTests, PendingNameIsNotShown) {
	nameCheck = GuildManager::NameCheck::PENDING;
	guilds->Create(ALICE, u"Unknown Words");
	EXPECT_EQ(To<ClientPackets::GuildCreateResponse>(ALICE).back().result, eGuildCreateResponse::CREATED);
	EXPECT_EQ(db.GetGuildByName("Unknown Words")->nameStatus, IGuilds::NAME_PENDING);
	EXPECT_EQ(To<ChatPackets::GuildStatus>(ALICE).back().guildName.GetAsString(), "");
	ASSERT_EQ(notes.size(), 1u);
	// The members see it in their guild window
	sent.clear();
	guilds->GetAll(ALICE);
	EXPECT_EQ(To<ClientPackets::GuildData>(ALICE).back().guildName, "Unknown Words");
}

TEST_F(GuildManagerTests, InviteAndAccept) {
	const auto guildID = MakeGuild({ BOB });
	guilds->Invite(ALICE, "carol");
	const auto initial = To<ClientPackets::GuildInviteInitialResponse>(ALICE);
	ASSERT_EQ(initial.size(), 1u);
	EXPECT_EQ(initial[0].response, eGuildInviteResponse::SENT);
	EXPECT_EQ(initial[0].playerName, "Carol");
	const auto invites = To<ClientPackets::GuildInvite>(CAROL);
	ASSERT_EQ(invites.size(), 1u);
	EXPECT_EQ(invites[0].inviterName, "Alice");
	EXPECT_EQ(invites[0].guildName, "Brick Builders");

	sent.clear();
	guilds->AnswerInvite(CAROL, false);
	const auto confirm = To<ClientPackets::GuildInviteConfirm>(CAROL);
	ASSERT_EQ(confirm.size(), 1u);
	EXPECT_FALSE(confirm[0].failed);
	EXPECT_EQ(db.GetGuildMember(CAROL)->rank, static_cast<uint8_t>(eGuildRank::RECRUIT));
	EXPECT_EQ(To<ChatPackets::GuildStatus>(CAROL).back().guildID, guildID);
	// The inviter hears the answer and gets the list again; the other members get GUILD_ADD_PLAYER
	EXPECT_EQ(To<ClientPackets::GuildInviteFinalResponse>(ALICE).back().response, eGuildInviteFinalResponse::JOINED);
	EXPECT_EQ(To<ClientPackets::GuildData>(ALICE).back().members.size(), 3u);
	EXPECT_TRUE(To<ClientPackets::GuildAddPlayer>(ALICE).empty());
	const auto added = To<ClientPackets::GuildAddPlayer>(BOB);
	ASSERT_EQ(added.size(), 1u);
	EXPECT_EQ(added[0].playerID, CAROL);
	EXPECT_EQ(added[0].rank, eGuildRank::RECRUIT);
	EXPECT_TRUE(added[0].online);
	EXPECT_FALSE(db.GetGuildInvite(CAROL));
}

TEST_F(GuildManagerTests, InviteRefusals) {
	MakeGuild({ BOB });
	const auto result = [&](LWOOBJID player) {
		const auto responses = To<ClientPackets::GuildInviteInitialResponse>(player);
		sent.clear();
		return responses.empty() ? eGuildInviteResponse::SENT : responses.back().response;
	};
	// Recruits can't invite
	guilds->Invite(BOB, "Carol");
	EXPECT_EQ(result(BOB), eGuildInviteResponse::FAILED);
	guilds->Invite(ALICE, "Erin");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::NOT_ONLINE);
	guilds->Invite(ALICE, "Nobody");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::FAILED);
	guilds->Invite(ALICE, "Bob");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::ALREADY_IN_GUILD);
	guilds->Invite(ALICE, "Alice");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::FAILED);
	guilds->Invite(ALICE, "Carol");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::SENT);
	guilds->Invite(ALICE, "Carol");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::INVITE_PENDING);
	// An invite runs out
	now += 601;
	guilds->Invite(ALICE, "Carol");
	EXPECT_EQ(result(ALICE), eGuildInviteResponse::SENT);
	// Someone not in a guild
	guilds->Invite(DAVE, "Carol");
	EXPECT_EQ(result(DAVE), eGuildInviteResponse::FAILED);
}

TEST_F(GuildManagerTests, GuildIsFull) {
	MakeGuild({ BOB, CAROL, DAVE });
	online[ERIN] = { ERIN, "Erin", LWOZONEID(1200, 1, 0) };
	guilds->Invite(ALICE, "Erin");
	EXPECT_EQ(To<ClientPackets::GuildInviteInitialResponse>(ALICE).back().response, eGuildInviteResponse::FAILED);
	EXPECT_FALSE(db.GetGuildMember(ERIN));
}

TEST_F(GuildManagerTests, DeclineAndExpiredAnswers) {
	MakeGuild();
	guilds->Invite(ALICE, "Bob");
	sent.clear();
	guilds->AnswerInvite(BOB, true);
	EXPECT_EQ(To<ClientPackets::GuildInviteFinalResponse>(ALICE).back().response, eGuildInviteFinalResponse::DECLINED);
	EXPECT_TRUE(To<ClientPackets::GuildInviteConfirm>(BOB).empty());
	EXPECT_FALSE(db.GetGuildMember(BOB));

	guilds->Invite(ALICE, "Bob");
	now += 601;
	sent.clear();
	guilds->AnswerInvite(BOB, false);
	EXPECT_TRUE(To<ClientPackets::GuildInviteConfirm>(BOB).back().failed);
	EXPECT_FALSE(db.GetGuildMember(BOB));

	// No invite at all
	sent.clear();
	guilds->AnswerInvite(CAROL, false);
	EXPECT_TRUE(To<ClientPackets::GuildInviteConfirm>(CAROL).back().failed);
}

TEST_F(GuildManagerTests, Data) {
	MakeGuild({ BOB, CAROL });
	online.erase(CAROL);
	guilds->GetAll(BOB);
	const auto data = To<ClientPackets::GuildData>(BOB);
	ASSERT_EQ(data.size(), 1u);
	EXPECT_EQ(data[0].status, 0);
	EXPECT_EQ(data[0].guildName, "Brick Builders");
	EXPECT_EQ(data[0].foundDate, "11/14/2023");
	ASSERT_EQ(data[0].members.size(), 3u);
	EXPECT_EQ(data[0].members[0].playerID, ALICE);
	EXPECT_EQ(data[0].members[0].rank, eGuildRank::LEADER);
	EXPECT_TRUE(data[0].members[0].online);
	EXPECT_EQ(data[0].members[0].zoneID, LWOZONEID(1200, 1, 0));
	EXPECT_EQ(data[0].members[2].name, "Carol");
	EXPECT_FALSE(data[0].members[2].online);
	EXPECT_EQ(data[0].members[2].zoneID, LWOZONEID(0, 0, 0));

	sent.clear();
	guilds->GetAll(DAVE);
	const auto none = To<ClientPackets::GuildData>(DAVE);
	ASSERT_EQ(none.size(), 1u);
	EXPECT_EQ(none[0].status, 1);
	EXPECT_TRUE(none[0].members.empty());
}

TEST_F(GuildManagerTests, LeaveHandsTheGuildOn) {
	const auto guildID = MakeGuild({ BOB, CAROL });
	guilds->SetRank(ALICE, "Carol", eGuildRank::OFFICER);
	sent.clear();
	guilds->Leave(ALICE);
	EXPECT_FALSE(db.GetGuildMember(ALICE));
	// Carol outranks Bob, who joined first
	EXPECT_EQ(db.GetGuildMember(CAROL)->rank, static_cast<uint8_t>(eGuildRank::LEADER));
	for (const auto id : { ALICE, BOB, CAROL }) {
		const auto removed = To<ClientPackets::GuildRemovePlayer>(id);
		ASSERT_EQ(removed.size(), 1u) << id;
		EXPECT_EQ(removed[0].reason, eGuildLeaveReason::LEFT);
		EXPECT_EQ(removed[0].playerID, ALICE);
		EXPECT_EQ(removed[0].newLeaderID, CAROL);
	}
	EXPECT_EQ(To<ChatPackets::GuildStatus>(ALICE).back().guildID, 0);

	guilds->Leave(BOB);
	sent.clear();
	guilds->Leave(CAROL);
	// The last one out: the guild is gone
	EXPECT_FALSE(db.GetGuild(guildID));
	EXPECT_EQ(To<ClientPackets::GuildRemovePlayer>(CAROL).back().newLeaderID, LWOOBJID_EMPTY);
	EXPECT_EQ(db.events.back().kind, "disbanded");
}

TEST_F(GuildManagerTests, Kick) {
	MakeGuild({ BOB, CAROL });
	guilds->SetRank(ALICE, "Bob", eGuildRank::OFFICER);
	notes.clear();
	sent.clear();
	// Officers can't kick officers or the leader, recruits nobody
	guilds->Kick(CAROL, "Bob");
	guilds->Kick(BOB, "Alice");
	EXPECT_EQ(notes.size(), 2u);
	EXPECT_TRUE(db.GetGuildMember(BOB));
	guilds->Kick(BOB, "carol");
	EXPECT_FALSE(db.GetGuildMember(CAROL));
	const auto removed = To<ClientPackets::GuildRemovePlayer>(CAROL);
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].reason, eGuildLeaveReason::KICKED);
	EXPECT_EQ(removed[0].playerID, CAROL);
	EXPECT_EQ(To<ClientPackets::GuildRemovePlayer>(ALICE).size(), 1u);
	EXPECT_EQ(db.events.back().kind, "kicked");
	EXPECT_EQ(db.events.back().actor, "Bob");
}

TEST_F(GuildManagerTests, Ranks) {
	MakeGuild({ BOB, CAROL });
	guilds->SetRank(ALICE, "Bob", eGuildRank::OFFICER);
	EXPECT_EQ(db.GetGuildMember(BOB)->rank, static_cast<uint8_t>(eGuildRank::OFFICER));
	// Everyone online gets the list again (the client has no rank change packet)
	EXPECT_EQ(To<ClientPackets::GuildData>(CAROL).size(), 1u);
	guilds->SetRank(BOB, "Carol", eGuildRank::VETERAN);
	EXPECT_EQ(db.GetGuildMember(CAROL)->rank, static_cast<uint8_t>(eGuildRank::VETERAN));
	guilds->SetRank(BOB, "Carol", eGuildRank::OFFICER);
	EXPECT_EQ(db.GetGuildMember(CAROL)->rank, static_cast<uint8_t>(eGuildRank::VETERAN));
	// Handing the guild over
	guilds->SetRank(ALICE, "Carol", eGuildRank::LEADER);
	EXPECT_EQ(db.GetGuildMember(CAROL)->rank, static_cast<uint8_t>(eGuildRank::LEADER));
	EXPECT_EQ(db.GetGuildMember(ALICE)->rank, static_cast<uint8_t>(eGuildRank::OFFICER));
}

TEST_F(GuildManagerTests, Disband) {
	const auto guildID = MakeGuild({ BOB, CAROL });
	online.erase(CAROL);
	guilds->Disband(BOB);
	EXPECT_TRUE(db.GetGuild(guildID));
	guilds->Disband(ALICE);
	EXPECT_FALSE(db.GetGuild(guildID));
	EXPECT_TRUE(db.members.empty());
	// Each online member is told they left, so their client drops the guild
	for (const auto id : { ALICE, BOB }) {
		const auto removed = To<ClientPackets::GuildRemovePlayer>(id);
		ASSERT_EQ(removed.size(), 1u);
		EXPECT_EQ(removed[0].playerID, id);
		EXPECT_EQ(To<ChatPackets::GuildStatus>(id).back().guildID, 0);
	}
	EXPECT_TRUE(To<ClientPackets::GuildRemovePlayer>(CAROL).empty());
}

TEST_F(GuildManagerTests, OnlineStatus) {
	MakeGuild({ BOB, CAROL });
	guilds->PlayerOffline(CAROL);
	online.erase(CAROL);
	auto updates = To<ClientPackets::GuildLoginLogout>(ALICE);
	ASSERT_EQ(updates.size(), 1u);
	EXPECT_FALSE(updates[0].online);
	EXPECT_EQ(updates[0].playerID, CAROL);
	EXPECT_EQ(To<ClientPackets::GuildLoginLogout>(BOB).size(), 1u);

	sent.clear();
	online[CAROL] = { CAROL, "Carol", LWOZONEID(1100, 5, 0) };
	guilds->PlayerOnline(CAROL, true);
	updates = To<ClientPackets::GuildLoginLogout>(ALICE);
	ASSERT_EQ(updates.size(), 1u);
	EXPECT_TRUE(updates[0].online);
	EXPECT_FALSE(updates[0].worldUpdateOnly);
	EXPECT_EQ(updates[0].zoneID, LWOZONEID(1100, 5, 0));
	EXPECT_TRUE(To<ClientPackets::GuildLoginLogout>(CAROL).empty());

	sent.clear();
	guilds->PlayerOnline(CAROL, false);
	EXPECT_TRUE(To<ClientPackets::GuildLoginLogout>(ALICE).back().worldUpdateOnly);
	EXPECT_EQ(guilds->OnlineGuildmates(CAROL).size(), 3u);
	EXPECT_TRUE(guilds->OnlineGuildmates(DAVE).empty());
}

TEST_F(GuildManagerTests, OfflinePlayersLoseTheirInvite) {
	MakeGuild();
	guilds->Invite(ALICE, "Bob");
	guilds->PlayerOffline(BOB);
	EXPECT_FALSE(db.GetGuildInvite(BOB));
}

TEST_F(GuildManagerTests, LeaderlessGuildIsRepaired) {
	const auto guildID = MakeGuild({ BOB, CAROL });
	// Alice's character was deleted
	db.members.erase(ALICE);
	online.erase(ALICE);
	guilds->GetAll(BOB);
	EXPECT_EQ(db.GetGuildMember(BOB)->rank, static_cast<uint8_t>(eGuildRank::LEADER));
	EXPECT_EQ(To<ClientPackets::GuildData>(BOB).back().members[0].rank, eGuildRank::LEADER);
	EXPECT_TRUE(db.GetGuild(guildID));
}

TEST_F(GuildManagerTests, DashboardChanges) {
	const auto guildID = MakeGuild({ BOB, CAROL });
	// Renamed and approved
	db.SetGuildName(guildID, "New Name", IGuilds::NAME_APPROVED);
	guilds->GuildChanged(guildID);
	for (const auto id : { ALICE, BOB, CAROL }) {
		EXPECT_EQ(To<ChatPackets::GuildStatus>(id).back().guildName.GetAsString(), "New Name");
		EXPECT_EQ(To<ClientPackets::GuildData>(id).back().guildName, "New Name");
	}
	// A member removed on the dashboard
	sent.clear();
	db.RemoveGuildMember(CAROL);
	guilds->GuildChanged(guildID);
	EXPECT_EQ(To<ClientPackets::GuildRemovePlayer>(CAROL).back().playerID, CAROL);
	EXPECT_EQ(To<ChatPackets::GuildStatus>(CAROL).back().guildID, 0);
	// Disbanded on the dashboard
	sent.clear();
	db.DeleteGuild(guildID);
	guilds->GuildChanged(guildID);
	for (const auto id : { ALICE, BOB }) {
		EXPECT_EQ(To<ClientPackets::GuildRemovePlayer>(id).back().playerID, id);
		EXPECT_EQ(To<ChatPackets::GuildStatus>(id).back().guildID, 0);
	}
	EXPECT_TRUE(To<ClientPackets::GuildRemovePlayer>(CAROL).empty());
}
