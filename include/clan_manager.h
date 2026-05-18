#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <string>

namespace app {

class UserManager;  // forward declaration

class ClanManager {
public:
    ClanManager(Storage& storage, UserManager& userMgr);

    int createClan(const std::string& name, int leaderId);
    void dissolveClan(int clanId);
    void addMember(int clanId, int userId);
    void removeMember(int clanId, int userId);
    void transferLeadership(int clanId, int newLeaderId);

    std::vector<Clan> getActiveClans() const;
    std::vector<Clan> getAllClans() const;
    std::vector<ClanMember> getClanMembers(int clanId) const;
    Clan getClan(int clanId) const;
    bool isClanActive(int clanId) const;

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace app
