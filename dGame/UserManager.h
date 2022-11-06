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
	std::map<SystemAddress, User*> m_Users;
	std::vector<User*> m_UsersToDelete;

	std::vector<std::string> m_FirstNames;
	std::vector<std::string> m_MiddleNames;
	std::vector<std::string> m_LastNames;
	std::vector<std::string> m_PreapprovedNames;
};

#endif // USERMANAGER_H
