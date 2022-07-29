#pragma once
#include "Entity.h"

class BrickDatabase
{
public:
	static BrickDatabase* Instance() {
		if (m_Address == nullptr) {
			m_Address = new BrickDatabase();
		}

		return m_Address;
	}

	std::vector<Brick>& GetBricks(const std::string& lxfmlPath);

	explicit BrickDatabase();

	~BrickDatabase();

private:
	std::unordered_map<std::string, std::vector<Brick>> m_Cache;

	static std::vector<Brick> emptyCache;

	static BrickDatabase* m_Address; //For singleton method

	/* data */
};
