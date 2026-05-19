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
	int applyToClan(int clanId, int userId);
	void processApplication(int applicationId, int handlerId, const std::string& action);

    std::vector<Clan> getActiveClans() const;
    std::vector<Clan> getAllClans() const;
    std::vector<ClanMember> getClanMembers(int clanId) const;
    Clan getClan(int clanId) const;
    bool isClanActive(int clanId) const;
	std::vector<ClanApplication> getMyApplications(int userId, std::optional<int> statusFilter = std::nullopt) const;
	std::vector<ClanApplication> getPendingApplications(int clanId, int leaderId) const;

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace app
