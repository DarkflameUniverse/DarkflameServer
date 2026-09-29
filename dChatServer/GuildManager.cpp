#include "GuildManager.h"

#include <algorithm>
#include <ctime>
#include <set>

#include "ChatPackets.h"
#include "ClientPackets.h"
#include "GeneralUtils.h"
#include "GuildNameRules.h"
#include "Logger.h"

namespace {
	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	const char* RankName(eGuildRank rank) {
		switch (rank) {
		case eGuildRank::LEADER: return "Leader";
		case eGuildRank::OFFICER: return "Officer";
		case eGuildRank::VETERAN: return "Veteran";
		case eGuildRank::RECRUIT: return "Recruit";
		default: return "no rank";
		}
	}

	eGuildRank Rank(const IGuilds::Member& member) { return static_cast<eGuildRank>(member.rank); }

	// The zone the client is given for a member it shows as offline
	const LWOZONEID NO_ZONE(0, 0, 0);
}

GuildManager::GuildManager(IGuilds& db, Hooks hooks, Settings settings) : m_Db(db), m_Hooks(std::move(hooks)), m_Settings(settings) {}

bool GuildManager::IsValidName(const std::u16string& name) {
	return GuildNameRules::IsValid(name);
}

bool GuildManager::CanInvite(const eGuildRank rank) {
	return rank == eGuildRank::LEADER || rank == eGuildRank::OFFICER;
}

bool GuildManager::CanKick(const eGuildRank actor, const eGuildRank target) {
	if (actor == eGuildRank::LEADER) return target != eGuildRank::LEADER;
	if (actor == eGuildRank::OFFICER) return target == eGuildRank::VETERAN || target == eGuildRank::RECRUIT;
	return false;
}

bool GuildManager::CanSetRank(const eGuildRank actor, const eGuildRank target, const eGuildRank newRank) {
	if (target == eGuildRank::LEADER || newRank == eGuildRank::NONE || newRank > eGuildRank::RECRUIT) return false;
	// Handing the guild over
	if (newRank == eGuildRank::LEADER) return actor == eGuildRank::LEADER;
	if (actor == eGuildRank::LEADER) return true;
	// Officers move members between veteran and recruit
	return actor == eGuildRank::OFFICER && (target == eGuildRank::VETERAN || target == eGuildRank::RECRUIT) &&
		(newRank == eGuildRank::VETERAN || newRank == eGuildRank::RECRUIT);
}

std::string GuildManager::ShownName(const IGuilds::Guild& guild) {
	return guild.nameStatus == IGuilds::NAME_APPROVED ? guild.name : "";
}

std::string GuildManager::FormatDate(const int64_t time) {
	const auto seconds = static_cast<std::time_t>(time);
	std::tm tm{};
	gmtime_r(&seconds, &tm);
	char buffer[16];
	std::strftime(buffer, sizeof(buffer), "%m/%d/%Y", &tm);
	return buffer;
}

std::string GuildManager::NameOf(const LWOOBJID playerID, const std::string& fallback) {
	const auto online = m_Hooks.findOnline(playerID);
	return online ? online->name : fallback;
}

void GuildManager::LogEvent(const int64_t guildID, const std::string& kind, const LWOOBJID characterID, const std::string& characterName, const std::string& actor, const std::string& detail) {
	m_Db.InsertGuildEvent({ 0, guildID, m_Hooks.now(), kind, characterID, characterName, actor, detail });
}

void GuildManager::SendStatus(const LWOOBJID characterID, const int64_t guildID, const std::string& shownName) {
	if (guildID == 0) m_OnlineGuild.erase(characterID);
	else m_OnlineGuild[characterID] = guildID;
	ChatPackets::GuildStatus status;
	status.characterID = characterID;
	status.guildID = guildID;
	status.guildName = ClientPackets::Guild::FixedName(shownName, ClientPackets::Guild::NAME_SIZE);
	m_Hooks.sendToWorldOf(characterID, status);
}

std::vector<IGuilds::Member> GuildManager::Members(const int64_t guildID) {
	auto members = m_Db.GetGuildMembers(guildID);
	if (members.empty()) {
		if (m_Db.GetGuild(guildID)) {
			LOG("Guild %lli has no members left; deleting it", guildID);
			m_Db.DeleteGuild(guildID);
			LogEvent(guildID, "disbanded", 0, "", "", "no members left");
		}
		return members;
	}
	const bool hasLeader = std::any_of(members.begin(), members.end(), [](const IGuilds::Member& member) { return Rank(member) == eGuildRank::LEADER; });
	if (!hasLeader) {
		// Ordered by rank, then who joined first
		auto& heir = members.front();
		LOG("Guild %lli has no leader (their character is gone); %llu leads it now", guildID, heir.characterId);
		m_Db.SetGuildMemberRank(heir.characterId, static_cast<uint8_t>(eGuildRank::LEADER));
		heir.rank = static_cast<uint8_t>(eGuildRank::LEADER);
		LogEvent(guildID, "leader", heir.characterId, heir.name, "", "the leader's character is gone");
	}
	return members;
}

std::optional<IGuilds::Member> GuildManager::FindMember(const std::vector<IGuilds::Member>& members, const std::string& name) const {
	const auto wanted = Lower(name);
	for (const auto& member : members) {
		if (Lower(member.name) == wanted) return member;
	}
	return std::nullopt;
}

std::vector<LWOOBJID> GuildManager::OnlineGuildmates(const LWOOBJID playerID) {
	std::vector<LWOOBJID> online;
	const auto member = m_Db.GetGuildMember(playerID);
	if (!member) return online;
	for (const auto& other : m_Db.GetGuildMembers(member->guildId)) {
		if (m_Hooks.findOnline(other.characterId)) online.push_back(other.characterId);
	}
	return online;
}

void GuildManager::Create(const LWOOBJID playerID, const std::u16string& requestedName) {
	const auto player = m_Hooks.findOnline(playerID);
	if (!player) return;

	// Spaces typed around the name are not part of it
	auto name = requestedName;
	while (!name.empty() && name.front() == u' ') name.erase(name.begin());
	while (!name.empty() && name.back() == u' ') name.pop_back();
	const auto utf8Name = GeneralUtils::UTF16ToWTF8(name);

	ClientPackets::GuildCreateResponse response;
	response.guildName = utf8Name;
	const auto respond = [&](const eGuildCreateResponse result) {
		response.result = result;
		m_Hooks.sendToPlayer(playerID, response);
	};

	if (m_Db.GetGuildMember(playerID)) return respond(eGuildCreateResponse::FAILED);
	if (!IsValidName(name)) return respond(eGuildCreateResponse::BAD_NAME);
	const auto check = m_Hooks.checkName(utf8Name);
	if (check == NameCheck::DENIED) return respond(eGuildCreateResponse::BAD_NAME);
	if (m_Db.GetGuildByName(utf8Name)) return respond(eGuildCreateResponse::EXISTS);

	const auto now = m_Hooks.now();
	const int32_t status = check == NameCheck::APPROVED ? IGuilds::NAME_APPROVED : IGuilds::NAME_PENDING;
	int64_t guildID = 0;
	try {
		guildID = m_Db.InsertGuild({ 0, utf8Name, status, playerID, now });
	} catch (std::exception& ex) {
		// Another guild took the name between the check and the insert
		LOG("Could not create guild %s: %s", utf8Name.c_str(), ex.what());
		return respond(eGuildCreateResponse::EXISTS);
	}
	if (guildID == 0) return respond(eGuildCreateResponse::FAILED);

	m_Db.AddGuildMember({ playerID, guildID, static_cast<uint8_t>(eGuildRank::LEADER), now });
	m_Db.DeleteGuildInvite(playerID);
	LogEvent(guildID, "created", playerID, player->name, player->name, utf8Name);
	LOG("%s created guild %lli: %s%s", player->name.c_str(), guildID, utf8Name.c_str(), status == IGuilds::NAME_PENDING ? " (name waits for moderation)" : "");

	response.guildID = guildID;
	respond(eGuildCreateResponse::CREATED);
	SendStatus(playerID, guildID, status == IGuilds::NAME_APPROVED ? utf8Name : "");
	if (status == IGuilds::NAME_PENDING) {
		m_Hooks.notify(playerID, "Other players will see your guild's name once a moderator has approved it.");
	}
}

void GuildManager::Invite(const LWOOBJID playerID, const std::string& targetName) {
	ClientPackets::GuildInviteInitialResponse response;
	response.playerName = targetName;
	const auto respond = [&](const eGuildInviteResponse result) {
		response.response = result;
		m_Hooks.sendToPlayer(playerID, response);
	};

	const auto inviter = m_Db.GetGuildMember(playerID);
	if (!inviter) return respond(eGuildInviteResponse::FAILED);
	if (!CanInvite(Rank(*inviter))) {
		m_Hooks.notify(playerID, "Only the guild's leader and officers can invite players.");
		return respond(eGuildInviteResponse::FAILED);
	}
	const auto target = m_Hooks.findOnlineByName(targetName);
	if (!target) return respond(m_Hooks.characterExists(targetName) ? eGuildInviteResponse::NOT_ONLINE : eGuildInviteResponse::FAILED);
	response.playerName = target->name;
	if (target->id == playerID) return respond(eGuildInviteResponse::FAILED);
	if (m_Db.GetGuildMember(target->id)) return respond(eGuildInviteResponse::ALREADY_IN_GUILD);
	const auto now = m_Hooks.now();
	const auto pending = m_Db.GetGuildInvite(target->id);
	if (pending && pending->createdAt + m_Settings.inviteTimeout > now) return respond(eGuildInviteResponse::INVITE_PENDING);
	const auto guild = m_Db.GetGuild(inviter->guildId);
	if (!guild) return respond(eGuildInviteResponse::FAILED);
	if (m_Db.GetGuildMembers(guild->id).size() >= m_Settings.maxMembers) {
		m_Hooks.notify(playerID, "Your guild is full.");
		return respond(eGuildInviteResponse::FAILED);
	}

	m_Db.SetGuildInvite({ target->id, guild->id, playerID, now });
	respond(eGuildInviteResponse::SENT);

	ClientPackets::GuildInvite invite;
	invite.inviterName = NameOf(playerID, inviter->name);
	invite.guildName = guild->name;
	m_Hooks.sendToPlayer(target->id, invite);
}

void GuildManager::AnswerInvite(const LWOOBJID playerID, const bool declined) {
	const auto invite = m_Db.GetGuildInvite(playerID);
	const auto confirmFailed = [&](const std::string& guildName) {
		if (declined) return;
		ClientPackets::GuildInviteConfirm confirm;
		confirm.failed = true;
		confirm.guildName = guildName;
		m_Hooks.sendToPlayer(playerID, confirm);
	};
	if (!invite) return confirmFailed("");
	m_Db.DeleteGuildInvite(playerID);

	const auto guild = m_Db.GetGuild(invite->guildId);
	if (!guild || invite->createdAt + m_Settings.inviteTimeout <= m_Hooks.now()) return confirmFailed(guild ? guild->name : "");

	const auto inviteeName = NameOf(playerID);
	ClientPackets::GuildInviteFinalResponse final;
	final.playerName = inviteeName;
	const auto tellInviter = [&](const eGuildInviteFinalResponse result) {
		final.response = result;
		if (m_Hooks.findOnline(invite->inviterId)) m_Hooks.sendToPlayer(invite->inviterId, final);
	};

	if (declined) return tellInviter(eGuildInviteFinalResponse::DECLINED);

	const auto members = Members(guild->id);
	if (members.empty() || m_Db.GetGuildMember(playerID) || members.size() >= m_Settings.maxMembers) {
		tellInviter(eGuildInviteFinalResponse::FAILED);
		return confirmFailed(guild->name);
	}

	const auto now = m_Hooks.now();
	m_Db.AddGuildMember({ playerID, guild->id, static_cast<uint8_t>(eGuildRank::RECRUIT), now });
	LogEvent(guild->id, "joined", playerID, inviteeName, NameOf(invite->inviterId));

	ClientPackets::GuildInviteConfirm confirm;
	confirm.guildName = guild->name;
	m_Hooks.sendToPlayer(playerID, confirm);
	SendStatus(playerID, guild->id, ShownName(*guild));

	const auto invitee = m_Hooks.findOnline(playerID);
	ClientPackets::GuildAddPlayer added;
	added.playerName = inviteeName;
	added.playerID = playerID;
	added.rank = eGuildRank::RECRUIT;
	added.online = invitee.has_value();
	added.zoneID = invitee ? invitee->zone : NO_ZONE;
	for (const auto& member : members) {
		if (member.characterId == invite->inviterId || !m_Hooks.findOnline(member.characterId)) continue;
		m_Hooks.sendToPlayer(member.characterId, added);
	}
	// The inviter hears the answer, then gets the list again (GUILD_ADD_PLAYER would say "has joined" a second time)
	if (m_Hooks.findOnline(invite->inviterId)) {
		tellInviter(eGuildInviteFinalResponse::JOINED);
		SendData(invite->inviterId);
	}
}

void GuildManager::RemoveMember(const IGuilds::Guild& guild, const IGuilds::Member& member, const eGuildLeaveReason reason, const std::string& actor) {
	const auto members = m_Db.GetGuildMembers(guild.id);
	m_Db.RemoveGuildMember(member.characterId);
	const auto memberName = NameOf(member.characterId, member.name);
	LogEvent(guild.id, reason == eGuildLeaveReason::KICKED ? "kicked" : "left", member.characterId, memberName, actor);

	LWOOBJID newLeader = LWOOBJID_EMPTY;
	bool othersLeft = false;
	for (const auto& other : members) {
		if (other.characterId == member.characterId) continue;
		othersLeft = true;
		// Ordered by rank, then who joined first: the first one left leads when the leader goes
		if (Rank(member) == eGuildRank::LEADER && newLeader == LWOOBJID_EMPTY) {
			newLeader = other.characterId;
			m_Db.SetGuildMemberRank(newLeader, static_cast<uint8_t>(eGuildRank::LEADER));
			LogEvent(guild.id, "leader", newLeader, NameOf(newLeader, other.name), actor, "the leader left");
		}
	}
	if (!othersLeft) {
		m_Db.DeleteGuild(guild.id);
		LogEvent(guild.id, "disbanded", member.characterId, memberName, actor, "the last member left");
		LOG("Guild %lli (%s) is gone: its last member left", guild.id, guild.name.c_str());
	}

	ClientPackets::GuildRemovePlayer removed;
	removed.reason = reason;
	removed.playerName = memberName;
	removed.playerID = member.characterId;
	removed.newLeaderID = newLeader;
	for (const auto& other : members) {
		if (m_Hooks.findOnline(other.characterId)) m_Hooks.sendToPlayer(other.characterId, removed);
	}
	if (m_Hooks.findOnline(member.characterId)) SendStatus(member.characterId, 0, "");
	else m_OnlineGuild.erase(member.characterId);
}

void GuildManager::Leave(const LWOOBJID playerID) {
	const auto member = m_Db.GetGuildMember(playerID);
	if (!member) return;
	const auto guild = m_Db.GetGuild(member->guildId);
	if (!guild) return;
	RemoveMember(*guild, *member, eGuildLeaveReason::LEFT, NameOf(playerID, member->name));
}

void GuildManager::Kick(const LWOOBJID playerID, const std::string& targetName) {
	const auto actor = m_Db.GetGuildMember(playerID);
	if (!actor) return m_Hooks.notify(playerID, "You are not in a guild.");
	const auto guild = m_Db.GetGuild(actor->guildId);
	if (!guild) return;
	const auto target = FindMember(Members(guild->id), targetName);
	if (!target) return m_Hooks.notify(playerID, targetName + " is not in your guild.");
	if (target->characterId == playerID) return m_Hooks.notify(playerID, "To leave the guild, use the guild window's leave button.");
	if (!CanKick(Rank(*actor), Rank(*target))) return m_Hooks.notify(playerID, "You can't remove " + target->name + " from the guild.");
	RemoveMember(*guild, *target, eGuildLeaveReason::KICKED, NameOf(playerID, actor->name));
}

void GuildManager::SetRank(const LWOOBJID playerID, const std::string& targetName, const eGuildRank rank) {
	const auto actor = m_Db.GetGuildMember(playerID);
	if (!actor) return m_Hooks.notify(playerID, "You are not in a guild.");
	const auto guild = m_Db.GetGuild(actor->guildId);
	if (!guild) return;
	const auto members = Members(guild->id);
	const auto target = FindMember(members, targetName);
	if (!target) return m_Hooks.notify(playerID, targetName + " is not in your guild.");
	// Members() may have just made the actor the leader
	const auto actorNow = FindMember(members, actor->name);
	const auto actorRank = actorNow ? Rank(*actorNow) : Rank(*actor);
	if (target->characterId == playerID || !CanSetRank(actorRank, Rank(*target), rank)) {
		return m_Hooks.notify(playerID, "You can't make " + target->name + " a " + RankName(rank) + ".");
	}
	if (Rank(*target) == rank) return m_Hooks.notify(playerID, target->name + " is already a " + RankName(rank) + ".");

	const auto actorName = NameOf(playerID, actor->name);
	m_Db.SetGuildMemberRank(target->characterId, static_cast<uint8_t>(rank));
	if (rank == eGuildRank::LEADER) {
		m_Db.SetGuildMemberRank(playerID, static_cast<uint8_t>(eGuildRank::OFFICER));
		LogEvent(guild->id, "leader", target->characterId, target->name, actorName, "handed over");
	} else {
		LogEvent(guild->id, "rank", target->characterId, target->name, actorName, RankName(rank));
	}
	m_Hooks.notify(playerID, target->name + " is now a " + RankName(rank) + ".");
	// The client has no rank change packet (GuildSetPlayerRank does nothing): send the list again
	SendDataToOnline(guild->id);
}

void GuildManager::Disband(const LWOOBJID playerID) {
	const auto actor = m_Db.GetGuildMember(playerID);
	if (!actor) return m_Hooks.notify(playerID, "You are not in a guild.");
	if (Rank(*actor) != eGuildRank::LEADER) return m_Hooks.notify(playerID, "Only the guild's leader can disband it.");
	const auto guild = m_Db.GetGuild(actor->guildId);
	if (!guild) return;
	const auto members = m_Db.GetGuildMembers(guild->id);
	m_Db.DeleteGuild(guild->id);
	const auto actorName = NameOf(playerID, actor->name);
	LogEvent(guild->id, "disbanded", playerID, actorName, actorName);
	LOG("%s disbanded guild %lli (%s)", actorName.c_str(), guild->id, guild->name.c_str());
	for (const auto& member : members) {
		if (!m_Hooks.findOnline(member.characterId)) {
			m_OnlineGuild.erase(member.characterId);
			continue;
		}
		// Each client drops the guild when it is told that it left
		ClientPackets::GuildRemovePlayer removed;
		removed.reason = eGuildLeaveReason::LEFT;
		removed.playerName = NameOf(member.characterId, member.name);
		removed.playerID = member.characterId;
		m_Hooks.sendToPlayer(member.characterId, removed);
		SendStatus(member.characterId, 0, "");
		m_Hooks.notify(member.characterId, "The guild " + guild->name + " was disbanded.");
	}
}

void GuildManager::SendData(const LWOOBJID playerID) {
	ClientPackets::GuildData data;
	const auto member = m_Db.GetGuildMember(playerID);
	const auto guild = member ? m_Db.GetGuild(member->guildId) : std::nullopt;
	const auto members = guild ? Members(guild->id) : std::vector<IGuilds::Member>{};
	if (!guild || members.empty()) {
		data.status = 1;
		m_Hooks.sendToPlayer(playerID, data);
		return;
	}
	data.guildName = guild->name;
	data.joinDate = FormatDate(member->joinedAt);
	data.foundDate = FormatDate(guild->createdAt);
	for (const auto& other : members) {
		const auto online = m_Hooks.findOnline(other.characterId);
		ClientPackets::GuildData::Member entry;
		entry.rank = Rank(other);
		entry.online = online.has_value();
		entry.zoneID = online ? online->zone : NO_ZONE;
		entry.playerID = other.characterId;
		entry.name = online ? online->name : other.name;
		data.members.push_back(std::move(entry));
	}
	m_Hooks.sendToPlayer(playerID, data);
}

void GuildManager::SendDataToOnline(const int64_t guildID) {
	for (const auto& member : m_Db.GetGuildMembers(guildID)) {
		if (m_Hooks.findOnline(member.characterId)) SendData(member.characterId);
	}
}

void GuildManager::GetAll(const LWOOBJID playerID) {
	SendData(playerID);
}

void GuildManager::PlayerOnline(const LWOOBJID playerID, const bool login) {
	const auto player = m_Hooks.findOnline(playerID);
	const auto member = m_Db.GetGuildMember(playerID);
	if (!player || !member) {
		m_OnlineGuild.erase(playerID);
		return;
	}
	m_OnlineGuild[playerID] = member->guildId;
	ClientPackets::GuildLoginLogout update;
	update.playerName = player->name;
	update.playerID = playerID;
	update.online = true;
	update.zoneID = player->zone;
	update.worldUpdateOnly = !login;
	for (const auto& other : m_Db.GetGuildMembers(member->guildId)) {
		if (other.characterId == playerID || !m_Hooks.findOnline(other.characterId)) continue;
		m_Hooks.sendToPlayer(other.characterId, update);
	}
}

void GuildManager::PlayerOffline(const LWOOBJID playerID) {
	// An invite is answered in a message box; it is gone with the player
	m_Db.DeleteGuildInvite(playerID);
	m_OnlineGuild.erase(playerID);
	const auto member = m_Db.GetGuildMember(playerID);
	if (!member) return;
	ClientPackets::GuildLoginLogout update;
	update.playerName = NameOf(playerID, member->name);
	update.playerID = playerID;
	update.online = false;
	update.zoneID = NO_ZONE;
	for (const auto& other : m_Db.GetGuildMembers(member->guildId)) {
		if (other.characterId == playerID || !m_Hooks.findOnline(other.characterId)) continue;
		m_Hooks.sendToPlayer(other.characterId, update);
	}
}

void GuildManager::GuildChanged(const int64_t guildID) {
	const auto guild = m_Db.GetGuild(guildID);
	const auto members = guild ? m_Db.GetGuildMembers(guildID) : std::vector<IGuilds::Member>{};
	std::set<LWOOBJID> memberIDs;
	for (const auto& member : members) memberIDs.insert(member.characterId);

	// Online players who were in it and aren't any more
	std::vector<LWOOBJID> removed;
	for (const auto& [playerID, playerGuild] : m_OnlineGuild) {
		if (playerGuild == guildID && !memberIDs.contains(playerID)) removed.push_back(playerID);
	}
	for (const auto playerID : removed) {
		if (m_Hooks.findOnline(playerID)) {
			ClientPackets::GuildRemovePlayer packet;
			packet.reason = eGuildLeaveReason::KICKED;
			packet.playerName = NameOf(playerID);
			packet.playerID = playerID;
			m_Hooks.sendToPlayer(playerID, packet);
			SendStatus(playerID, 0, "");
			m_Hooks.notify(playerID, guild ? "A moderator removed you from your guild." : "A moderator disbanded your guild.");
		} else {
			m_OnlineGuild.erase(playerID);
		}
	}

	if (!guild) return;
	const auto shown = ShownName(*guild);
	for (const auto& member : members) {
		if (!m_Hooks.findOnline(member.characterId)) continue;
		SendStatus(member.characterId, guildID, shown);
		SendData(member.characterId);
	}
}
