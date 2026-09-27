#include "Replayer.h"

#include <cstring>
#include <map>
#include <thread>

#include "AuthPackets.h"
#include "ClientPackets.h"
#include "CommonPackets.h"
#include "FakeClient.h"
#include "MessageIdentifiers.h"
#include "PacketDecoder.h"
#include "ServiceType.h"
#include "WorldPackets.h"
#include "eLoginResponse.h"

using json = nlohmann::json;
using Record = CaptureBundle::Record;
using namespace std::chrono_literals;

namespace {
	struct Segment {
		eCaptureSource source{};
		std::vector<const Record*> records;
		bool characterSelect{};
	};

	std::string NameOf(const Record& record) { return PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header)).name; }
	std::string NameOf(const std::string& bytes) { return PacketDecoder::Decode(bytes, false).name; }

	template<typename T>
	std::string Bytes(const T& packet) {
		RakNet::BitStream stream;
		packet.WritePacket(stream);
		return std::string(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed());
	}

	template<typename T>
	bool Read(const std::string& bytes, T& packet) {
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		return packet.ReadHeader(stream) && packet.Deserialize(stream);
	}

	void ReplaceAll(std::string& bytes, int64_t from, int64_t to) {
		if (from == 0 || from == to) return;
		char a[8], b[8];
		std::memcpy(a, &from, 8);
		std::memcpy(b, &to, 8);
		const std::string_view needle(a, 8);
		for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 8)) std::memcpy(bytes.data() + at, b, 8);
	}

	// A replica construction's object and LOT: [ID][bit][u16 network ID][i64 object][i32 LOT]
	bool Construction(const std::string& bytes, int64_t& object, int32_t& lot) {
		if (bytes.empty() || static_cast<uint8_t>(bytes[0]) != ID_REPLICA_MANAGER_CONSTRUCTION) return false;
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		stream.IgnoreBytes(1);
		bool flag{};
		uint16_t network{};
		return stream.Read(flag) && stream.Read(network) && stream.Read(object) && stream.Read(lot);
	}

	std::vector<Segment> Split(const std::vector<Record>& records) {
		std::vector<Segment> segments;
		for (const auto& record : records) {
			const auto source = static_cast<eCaptureSource>(record.header.source);
			if ((source != eCaptureSource::AUTH && source != eCaptureSource::WORLD) || (record.header.flags & (PacketRecordFlags::MASTER_LINK | PacketRecordFlags::GAP))) continue;
			// RakNet's own connection messages (connected, disconnected) aren't sent: the fake client's RakNet makes its own
			if (CaptureTools::FromClient(record.header) && (record.bytes.empty() || static_cast<uint8_t>(record.bytes[0]) != ID_USER_PACKET_ENUM)) continue;
			const bool handshake = CaptureTools::FromClient(record.header) && NameOf(record) == "VERSION_CONFIRM";
			if (segments.empty() || handshake || segments.back().source != source) segments.push_back({ source, {}, false });
			segments.back().records.push_back(&record);
			const auto name = NameOf(record);
			if (source == eCaptureSource::WORLD && (name == "CHARACTER_LIST_REQUEST" || name == "CHARACTER_LIST_RESPONSE" || name == "LOGIN_REQUEST")) segments.back().characterSelect = true;
		}
		return segments;
	}

	class Run {
	public:
		Run(const CaptureBundle::Bundle& bundle, const Replayer::Options& options, Replayer::Result& result) : m_Bundle(bundle), m_Options(options), m_Result(result) {}

		void Go() {
			// Kept for the whole run: the segments and the expected answers point into it
			m_Records = m_Bundle.records;
			CaptureTools::SortTimeline(m_Records);
			const auto segments = Split(m_Records);
			m_Result.connections = segments.size();
			if (segments.empty()) return Stop("The bundle has no client connections");

			size_t first = 0;
			if (segments.front().source == eCaptureSource::AUTH) {
				if (!ReplaySegment(segments.front(), m_Options.host, m_Options.authPort)) return;
				first = 1;
			} else if (!Login()) {
				return;
			}
			for (size_t i = first; i < segments.size(); i++) {
				const auto& segment = segments[i];
				if (segment.source == eCaptureSource::AUTH) {
					if (!ReplaySegment(segment, m_Options.host, m_Options.authPort)) return;
					continue;
				}
				if (segment.characterSelect) {
					if (!ReplaySegment(segment, m_WorldHost, m_WorldPort)) return;
					continue;
				}
				// A zone: the target says where, after character select (done here if the recording didn't)
				if (!m_TransferPort && !CharacterSelect()) return;
				const auto port = m_TransferPort;
				m_TransferPort = 0;
				if (!ReplaySegment(segment, m_TransferHost, port)) return;
			}
		}

	private:
		const CaptureBundle::Bundle& m_Bundle;
		std::vector<Record> m_Records;
		const Replayer::Options& m_Options;
		Replayer::Result& m_Result;
		std::vector<const Record*> m_Expected;
		std::string m_UserKey;
		std::string m_WorldHost, m_TransferHost;
		uint16_t m_WorldPort{}, m_TransferPort{};
		std::map<int64_t, int64_t> m_Objects; // recorded object -> the target's, learned from constructions
		std::map<int32_t, std::vector<int64_t>> m_Recorded, m_Replayed; // constructions by LOT, in order
		std::map<int32_t, size_t> m_Paired;

		void Stop(const std::string& why) {
			if (m_Result.stoppedAt.empty()) m_Result.stoppedAt = why;
		}

		std::string Host(const std::string& ip) const {
			return ip.empty() || ip == "localhost" ? m_Options.host : ip;
		}

		void Handshake(FakeClient& client) {
			CommonPackets::ClientVersionConfirm version;
			version.netVersion = CommonPackets::ServerVersionConfirm::DEFAULT_NET_VERSION;
			version.serviceType = ServiceType::CLIENT;
			version.processID = 1;
			version.port = client.GetLocalPort();
			client.Send(Bytes(version));
			client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "VERSION_CONFIRM"; }, 0, 10s);
		}

		bool ReadLoginResponse(const std::string& bytes) {
			ClientPackets::LoginResponse response;
			if (!Read(bytes, response)) return false;
			if (response.responseCode != eLoginResponse::SUCCESS) {
				Stop("The target refused the login (response " + std::to_string(static_cast<int>(response.responseCode)) + ")");
				return false;
			}
			m_UserKey = response.userKey.GetAsString();
			m_WorldHost = Host(response.worldServerIP.string);
			m_WorldPort = response.worldServerPort;
			m_Result.loggedIn = true;
			return true;
		}

		// The recording has no login: log in on the target with the replay's account (not compared)
		bool Login() {
			FakeClient client;
			if (!client.Connect(m_Options.host, m_Options.authPort, 10s)) {
				Stop("Can't connect to auth at " + m_Options.host + ":" + std::to_string(m_Options.authPort));
				return false;
			}
			Handshake(client);
			AuthPackets::LoginRequest login;
			login.username = LUWString(m_Options.username);
			login.password = LUWString(m_Options.password, 41);
			client.Send(Bytes(login));
			const auto at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "LOGIN_RESPONSE"; }, 0, 15s);
			if (at < 0) {
				Stop("No login response from the target");
				return false;
			}
			m_Result.notes.push_back("Logged in on the target first (the recording starts after the login)");
			return ReadLoginResponse(client.GetReceived()[at].bytes);
		}

		// The recording starts in a zone: pick the character on the target (not compared)
		bool CharacterSelect() {
			FakeClient client;
			if (!client.Connect(m_WorldHost, m_WorldPort, 10s)) {
				Stop("Can't connect to character select at " + m_WorldHost + ":" + std::to_string(m_WorldPort));
				return false;
			}
			Handshake(client);
			WorldPackets::Validation validation;
			validation.username = LUWString(m_Options.username);
			validation.sessionKey = LUWString(m_UserKey);
			client.Send(Bytes(validation));
			client.Send(Bytes(WorldPackets::CharacterListRequest()));
			auto at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "CHARACTER_LIST_RESPONSE"; }, 0, 15s);
			if (at < 0) {
				Stop("No character list from the target");
				return false;
			}
			auto character = m_Options.character;
			if (!character) {
				// The bundle brought no character (it starts in the middle of a zone): make a plain one with the server's own code
				ClientPackets::CharacterListResponse list;
				if (Read(client.GetReceived()[at].bytes, list) && list.characters.empty()) {
					WorldPackets::CharacterCreateRequest create;
					create.name = LUWString(std::string("Replay"));
					client.Send(Bytes(create));
					client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "CHARACTER_CREATE_RESPONSE"; }, 0, 15s);
					const auto from = client.GetReceived().size();
					client.Send(Bytes(WorldPackets::CharacterListRequest()));
					at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "CHARACTER_LIST_RESPONSE"; }, from, 15s);
					if (at < 0 || !Read(client.GetReceived()[at].bytes, list)) {
						Stop("Couldn't make a character on the target");
						return false;
					}
					m_Result.notes.push_back("The bundle has no character data: replayed with a new plain character");
				}
				if (list.characters.empty()) {
					Stop("No character to play on the target");
					return false;
				}
				character = list.characters.front().objectID;
			}
			WorldPackets::CharacterLoginRequest pick;
			pick.playerID = character;
			client.Send(Bytes(pick));
			at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "TRANSFER_TO_WORLD"; }, 0, 180s);
			if (at < 0) {
				Stop("The target didn't send the character to a zone");
				return false;
			}
			ClientPackets::TransferToWorld transfer;
			if (!Read(client.GetReceived()[at].bytes, transfer)) return false;
			m_TransferHost = Host(transfer.serverIP.string);
			m_TransferPort = transfer.serverPort;
			m_Result.notes.push_back("Picked the character on the target first (the recording starts in a zone)");
			return true;
		}

		// What goes out in place of a recorded packet: the target's account, session key and IDs
		std::string Rewrite(const Record& record) {
			auto bytes = record.bytes;
			const auto name = NameOf(record);
			if (name == "LOGIN_REQUEST" && record.header.source == static_cast<uint8_t>(eCaptureSource::AUTH)) {
				AuthPackets::LoginRequest login;
				if (Read(bytes, login)) {
					login.username = LUWString(m_Options.username);
					login.password = LUWString(m_Options.password, 41);
					bytes = Bytes(login);
				}
			} else if (name == "VALIDATION") {
				WorldPackets::Validation validation;
				if (Read(bytes, validation)) {
					validation.username = LUWString(m_Options.username);
					validation.sessionKey = LUWString(m_UserKey);
					bytes = Bytes(validation);
				}
			}
			for (const auto& [from, to] : m_Options.ids) ReplaceAll(bytes, from, to);
			for (const auto& [from, to] : m_Objects) ReplaceAll(bytes, from, to);
			return bytes;
		}

		// Pairs the objects the target made with the recorded ones, by LOT and order
		void Learn(const FakeClient& client, size_t& seen) {
			const auto& received = client.GetReceived();
			for (; seen < received.size(); seen++) {
				int64_t object{};
				int32_t lot{};
				if (Construction(received[seen].bytes, object, lot)) m_Replayed[lot].push_back(object);
			}
			for (auto& [lot, objects] : m_Recorded) {
				auto& paired = m_Paired[lot];
				const auto& other = m_Replayed[lot];
				for (; paired < objects.size() && paired < other.size(); paired++) {
					if (objects[paired] != other[paired]) m_Objects[objects[paired]] = other[paired];
				}
			}
		}

		bool ReplaySegment(const Segment& segment, const std::string& host, uint16_t port) {
			FakeClient client;
			if (port == 0 || !client.Connect(host, port, 15s)) {
				Stop("Can't connect to " + host + ":" + std::to_string(port) + (segment.source == eCaptureSource::AUTH ? " (auth)" : " (world)"));
				return false;
			}
			m_Result.connectionsReached++;
			const auto zone = segment.records.front()->header.zoneId;
			int64_t previous = segment.records.front()->header.timeUs;
			size_t learned = 0;
			bool ok = true;
			// The answers recorded so far on this connection, by name, and the last one: a client packet goes out once the
			// target has sent what the recorded server had sent before it (a real client reacts to those)
			std::map<std::string, size_t> recordedCounts;
			std::string lastAnswer;
			std::map<std::string, bool> slow; // answers the target didn't send in time once: not waited for long again
			for (const auto* record : segment.records) {
				if (!CaptureTools::FromClient(record->header)) {
					lastAnswer = NameOf(*record);
					recordedCounts[lastAnswer]++;
					m_Expected.push_back(record);
					int64_t object{};
					int32_t lot{};
					if (Construction(record->bytes, object, lot)) m_Recorded[lot].push_back(object);
					continue;
				}
				const auto gap = std::min<int64_t>(static_cast<int64_t>((record->header.timeUs - previous) / 1000 / m_Options.speed), m_Options.maxGapMs);
				previous = record->header.timeUs;
				if (gap > 0) client.Idle(std::chrono::milliseconds(gap));
				// Wait for what a real client waits for before this
				const auto name = NameOf(*record);
				const auto waitFor = [&](const char* answer, std::chrono::seconds timeout) {
					if (client.WaitFor([answer](const auto& r) { return NameOf(r.bytes) == answer; }, 0, timeout) < 0) {
						m_Result.notes.push_back(std::string("No ") + answer + " from the target before " + name);
					}
				};
				if (name == "VALIDATION" || (name == "LOGIN_REQUEST" && segment.source == eCaptureSource::AUTH)) waitFor("VERSION_CONFIRM", 10s);
				else if (name == "LOGIN_REQUEST") waitFor("CHARACTER_LIST_RESPONSE", 10s);
				else if (name == "LEVEL_LOAD_COMPLETE") waitFor("LOAD_STATIC_ZONE", 30s);
				if (!lastAnswer.empty()) {
					const auto want = recordedCounts[lastAnswer];
					size_t have = 0;
					const auto enough = [&](const auto& r) { return NameOf(r.bytes) == lastAnswer && ++have >= want; };
					if (client.WaitFor(enough, 0, slow[lastAnswer] ? 500ms : 10s) < 0 && !slow[lastAnswer]) {
						slow[lastAnswer] = true;
						m_Result.notes.push_back("Waited in vain for " + lastAnswer + " (" + std::to_string(want) + " recorded by then) before " + name);
					}
				}
				Learn(client, learned);
				if (!client.Pump()) {
					Stop("The target closed the connection before " + name);
					ok = false;
					break;
				}
				client.Send(Rewrite(*record));
				m_Result.sent++;
			}
			// The last answers; and where to go next
			if (ok) {
				if (segment.source == eCaptureSource::AUTH) {
					const auto at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "LOGIN_RESPONSE"; }, 0, 15s);
					if (at < 0 || !ReadLoginResponse(client.GetReceived()[at].bytes)) {
						Stop(m_Result.stoppedAt.empty() ? "No login response from the target" : m_Result.stoppedAt);
						ok = false;
					}
				} else {
					// Until the target has been quiet a while (it may still be sending the zone)
					for (size_t count = client.GetReceived().size(), rounds = 0; rounds < 30; rounds++) {
						client.Idle(1s);
						if (client.GetReceived().size() == count && rounds >= 3) break;
						count = client.GetReceived().size();
					}
					// A recorded move to another world is waited for as long as a zone takes to start
					const auto at = client.WaitFor([](const auto& r) { return NameOf(r.bytes) == "TRANSFER_TO_WORLD"; }, 0,
						recordedCounts.contains("TRANSFER_TO_WORLD") ? std::chrono::seconds(180) : std::chrono::seconds(1));
					if (at >= 0) {
						ClientPackets::TransferToWorld transfer;
						if (Read(client.GetReceived()[at].bytes, transfer)) {
							m_TransferHost = Host(transfer.serverIP.string);
							m_TransferPort = transfer.serverPort;
						}
					}
				}
			}
			for (const auto& received : client.GetReceived()) {
				Record record;
				record.bytes = received.bytes;
				record.header.timeUs = received.timeUs;
				record.header.source = static_cast<uint8_t>(segment.source);
				record.header.direction = static_cast<uint8_t>(ePacketDirection::SENT);
				record.header.zoneId = zone;
				record.header.length = static_cast<uint32_t>(record.bytes.size());
				record.header.bits = record.header.length * 8;
				m_Result.actual.push_back(std::move(record));
			}
			m_Result.received += client.GetReceived().size();
			return ok;
		}

	public:
		std::vector<Record> Expected() const {
			std::vector<Record> out;
			for (const auto* record : m_Expected) out.push_back(*record);
			return out;
		}
	};
}

namespace Replayer {
	json Result::ToJson() const {
		return { {"loggedIn", loggedIn}, {"connections", connections}, {"connectionsReached", connectionsReached}, {"sent", sent}, {"received", received},
			{"stoppedAt", stoppedAt}, {"notes", notes}, {"diff", diff.ToJson()} };
	}

	Result Replay(const CaptureBundle::Bundle& bundle, const Options& options) {
		Result result;
		Run run(bundle, options, result);
		run.Go();
		const auto expected = run.Expected();
		result.diff = CaptureTools::Diff(expected, result.actual);

		// Zone data that differs is reported, not taken for a server difference
		std::map<int, json> recordedZones, replayedZones;
		for (const auto& record : expected) {
			const auto decoded = PacketDecoder::Decode(record.bytes, false);
			if (decoded.name == "LOAD_STATIC_ZONE" && decoded.fields) recordedZones[(*decoded.fields)["mapID"].get<int>()] = (*decoded.fields)["mapChecksum"];
		}
		for (const auto& record : result.actual) {
			const auto decoded = PacketDecoder::Decode(record.bytes, false);
			if (decoded.name == "LOAD_STATIC_ZONE" && decoded.fields) replayedZones[(*decoded.fields)["mapID"].get<int>()] = (*decoded.fields)["mapChecksum"];
		}
		for (const auto& [zone, checksum] : recordedZones) {
			const auto it = replayedZones.find(zone);
			if (it != replayedZones.end() && it->second != checksum) {
				result.notes.push_back("Zone " + std::to_string(zone) + " has other data on the target (checksum " + it->second.dump() + ", recorded " + checksum.dump() + ")");
			}
		}
		return result;
	}
}
