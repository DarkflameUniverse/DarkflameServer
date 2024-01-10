#include "GameMessage.h"

#include "AssetManager.h"
#include <BinaryPathFinder.h>

#include "dServer.h"

GameMessageStorage::GameMessageStorage() {
	auto path = BinaryPathFinder::GetBinaryDir() / "gms.json";

	std::ifstream i(path);
	i >> m_Storage;
	i.close();
}

uint32_t GameMessageStorage::GetGMFromName(std::string name) {
	auto messages = m_Storage["messages"];

	for (auto& [key, value] : messages.items()) {
		if (value["name"] == name) {
			return std::stoi(key);
		}
	}

	return 0;
}

nlohmann::json GameMessageStorage::GetGM(uint32_t id) {
	auto messages = m_Storage["messages"];

	if (messages.contains(std::to_string(id))) {
		return messages[std::to_string(id)];
	}

	return nlohmann::json();
}


GameMessageStorage::~GameMessageStorage() {
	
}

GameMessage::GameMessage(std::string name) {
	auto id = GameMessageStorage::Instance().GetGMFromName(name);
	if (id == 0) {
		throw std::exception("GameMessage not found");
	}

	new(this) GameMessage((eGameMessageType)id);
}

GameMessage::GameMessage(eGameMessageType type) {
	m_Type = type;
	m_Message = GameMessageStorage::Instance().GetGM((uint32_t)type);

	m_State = std::map<std::string, std::any>();
}

void GameMessage::Serialize(RakNet::BitStream* bs) {
	bs->Write<uint16_t>((uint16_t)m_Type);

	for (auto& value : m_Message["params"]) {
		bool presentInState = m_State.contains(value["name"]);

		if (value["type"] != "bool") {
			if (!presentInState) {
				if (value.contains("default")) {

					LOG("Written type %s as default", value["name"].get<std::string>().c_str());
					bs->Write0();
					continue;
				} else {
					throw std::exception("Fuck.");
				}
			} else {
				if (value.contains("default") && value["type"] != "bool") {
					bs->Write1();
				}
			}
		} else {
			// special case for bool

			if (!presentInState) {
				if (!value.contains("default")) throw std::exception("uh oh");

				m_State.insert(std::make_pair(value["name"].get<std::string>(), std::any(value["default"] == "true" ? true : false)));
			}
		}

		std::string type = value["type"];

		if (type == "Vector3") {
			auto res = std::any_cast<Vector3>(m_State[value["name"]]);
			bs->Write(res.GetX());
			bs->Write(res.GetY());
			bs->Write(res.GetZ());
			LOG("Written type %s with values %f %f %f", value["name"].get<std::string>().c_str(), res.GetX(), res.GetY(), res.GetZ());
		} else if (type == "float") {
			bs->Write<float>(std::any_cast<float>(m_State[value["name"]]));
			LOG("Written type %s with value %s", value["name"].get<std::string>().c_str(), std::to_string(std::any_cast<float>(m_State[value["name"]])).c_str());
		} else if (type == "bool") {
			bs->Write(std::any_cast<bool>(m_State[value["name"]]));
			LOG("Written type %s with value %s", value["name"].get<std::string>().c_str(), std::to_string(std::any_cast<bool>(m_State[value["name"]])).c_str());
		}
	}
}

