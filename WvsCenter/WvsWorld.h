#pragma once
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "..\WvsLib\Net\WorldInfo.h"
#include "..\WvsLib\Common\ConfigLoader.hpp"

class UserTransferStatus;
class User;

class WvsWorld
{
public:
	const static int 
		CHANNELID_NOT_MIGRATED_IN = -2,
		CHANNELID_SHOP = -1;

	struct WorldUser
	{
		bool m_bMigrating = false, 
			m_bLoggedIn = false,
			m_bMigrated = false,
			m_bInShop = false;

		int m_nCharacterID = 0, 
			m_nLocalSrvIdx = 0, 
			m_nChannelID = 0, 
			m_nLocalSocketSN = 0,
			m_nAccountID = 0;
	};

private:
	std::mutex m_mtxWorldLock;
	ConfigLoader* m_pCfgLoader;
	WorldInfo m_WorldInfo;

	std::map<int, WorldUser*> m_mUser;
	std::map<int, std::shared_ptr<UserTransferStatus>> m_mUserTransferStatus;

public:
	WvsWorld();
	~WvsWorld();

	void SetConfigLoader(ConfigLoader* pCfg);
	void InitializeWorld();
	const WorldInfo& GetWorldInfo() const;
	static WvsWorld* GetInstance();
	void SetUserTransferStatus(int nUserID, UserTransferStatus* pStatus);
	const UserTransferStatus* GetUserTransferStatus(int nUserID) const;
	void ClearUserTransferStatus(int nUserID);
	void UserMigrateIn(int nCharacterID, int nChannelID);
	void RemoveUser(int nUserID, int nIdx, int nLocalSocketSN, bool bMigrate);
	void SetUser(int nUserID, WorldUser *pWorldUser);
	WorldUser* GetUser(int nUserID);
};
