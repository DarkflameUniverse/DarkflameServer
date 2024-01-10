#pragma once

#include <any>

#include <nlohmann/json.hpp>
#include "Singleton.h"
#include "eGameMessageType.h"

typedef std::map<std::string, std::any> StateStorage;

class GameMessageStorage : public Singleton<GameMessageStorage> {
public:
	GameMessageStorage();
	~GameMessageStorage();

	uint32_t GetGMFromName(std::string name);
	nlohmann::json GetGM(uint32_t id);
private:
	nlohmann::json m_Storage;
};

class GameMessage {
public:
	GameMessage(std::string name);
	GameMessage(eGameMessageType type);

	void Serialize(RakNet::BitStream* bs);

	template <typename T>
	inline void Set(std::string key, T value) {
		m_State[key] = std::any(value);
	}

	template <typename T>
	inline T Get(std::string key) {
		return m_State[key];
	}

	inline eGameMessageType GetType() {
		return m_Type;
	}

	inline StateStorage GetState() {
		return m_State;
	}

	inline nlohmann::json GetMessage() {
		return m_Message;
	}

private:
	eGameMessageType m_Type;
	nlohmann::json m_Message;

	StateStorage m_State;
};
