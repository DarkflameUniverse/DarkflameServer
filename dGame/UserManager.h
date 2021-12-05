#ifndef USERMANAGER_H
#define USERMANAGER_H

#define _VARIADIC_MAX 10
#include <string>
#include <vector>
#include "RakNetTypes.h"
#include <map>

class User;

class UserManager {
public:
	static UserManager* Instance() {
		if (!m_Address) {
			m_Address = new UserManager();
		}
		
		return m_Address;
	}
	
	void Initialize();
	~UserManager();
	
	User* CreateUser(const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey);
	User* GetUser(const SystemAddress& sysAddr);
	User* GetUser(const std::string& username);
	bool DeleteUser(const SystemAddress& sysAddr); //Returns true on succesful deletion
	void DeletePendingRemovals();
	
	bool IsNameAvailable(const std::string& requestedName);
	std::string GetPredefinedName(uint32_t firstNameIndex, uint32_t middleNameIndex, uint32_t lastNameIndex);
	bool IsNamePreapproved(const std::string& requestedName);
	
	void RequestCharacterList(const SystemAddress& sysAddr);
    void CreateCharacter(const SystemAddress& sysAddr, Packet* packet);
    void DeleteCharacter(const SystemAddress& sysAddr, Packet* packet);
    void RenameCharacter(const SystemAddress& sysAddr, Packet* packet);
    void LoginCharacter(const SystemAddress& sysAddr, uint32_t playerID);

	void SaveAllActiveCharacters();

	size_t GetUserCount() const { return m_Users.size(); }

private:
	static UserManager* m_Address; //Singleton
	//std::vector<User*> m_Users;
	std::map<SystemAddress, User*> m_Users;
	std::vector<User*> m_UsersToDelete;
	
    std::vector<std::string> m_FirstNames;
    std::vector<std::string> m_MiddleNames;
    std::vector<std::string> m_LastNames;
	std::vector<std::string> m_PreapprovedNames;
};

enum CharCreatePantsColor : uint32_t {
	PANTS_BRIGHT_RED = 2508,
	PANTS_BRIGHT_ORANGE = 2509,
	PANTS_BRICK_YELLOW = 2511,
	PANTS_MEDIUM_BLUE = 2513,
	PANTS_SAND_GREEN = 2514,
	PANTS_DARK_GREEN = 2515,
	PANTS_EARTH_GREEN = 2516,
	PANTS_EARTH_BLUE = 2517,
	PANTS_BRIGHT_BLUE = 2519,
	PANTS_SAND_BLUE = 2520,
	PANTS_DARK_STONE_GRAY = 2521,
	PANTS_MEDIUM_STONE_GRAY = 2522,
	PANTS_WHITE = 2523,
	PANTS_BLACK = 2524,
	PANTS_REDDISH_BROWN = 2526,
	PANTS_DARK_RED = 2527
};

enum CharCreateShirtColor : uint32_t {
	SHIRT_BRIGHT_RED = 4049,
	SHIRT_BRIGHT_BLUE = 4083,
	SHIRT_BRIGHT_YELLOW = 4117,
	SHIRT_DARK_GREEN = 4151,
	SHIRT_BRIGHT_ORANGE = 4185,
	SHIRT_BLACK = 4219,
	SHIRT_DARK_STONE_GRAY = 4253,
	SHIRT_MEDIUM_STONE_GRAY = 4287,
	SHIRT_REDDISH_BROWN = 4321,
	SHIRT_WHITE = 4355,
	SHIRT_MEDIUM_BLUE = 4389,
	SHIRT_DARK_RED = 4423,
	SHIRT_EARTH_BLUE = 4457,
	SHIRT_EARTH_GREEN = 4491,
	SHIRT_BRICK_YELLOW = 4525,
	SHIRT_SAND_BLUE = 4559,
	SHIRT_SAND_GREEN = 4593
};

#endif // USERMANAGER_H
