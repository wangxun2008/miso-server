#include "clan_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

ClanManager::ClanManager(Storage& storage, UserManager& userMgr)
    : storage(storage), userMgr(userMgr) {}

int ClanManager::createClan(const std::string& name, int leaderId) {
    if (!userMgr.isUserActive(leaderId)) {
        throw UserNotFoundException("ID " + std::to_string(leaderId));
    }
    auto existing = storage.get_all<Clan>(
        where(c(&Clan::name) == name and c(&Clan::deleted_at) == 0)
    );
    if (!existing.empty()) {
        throw ClanNameConflictException(name);
    }

    auto guard = storage.transaction_guard();
    Clan clan{-1, name, leaderId, 0};
    clan.id = storage.insert(clan);

    ClanMember cm{-1, clan.id, leaderId};
    storage.insert(cm);

    guard.commit();
    return clan.id;
}

void ClanManager::dissolveClan(int clanId) {
    Clan clan = getClan(clanId); // 确保是活跃战队
    clan.deleted_at = getCurrentTimestamp();
    storage.update(clan);
}

void ClanManager::addMember(int clanId, int userId) {
    if (!isClanActive(clanId)) {
        throw ClanNotActiveException(clanId);
    }
    if (!userMgr.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    auto existing = storage.get_all<ClanMember>(
        where(c(&ClanMember::clan_id) == clanId
              and c(&ClanMember::user_id) == userId)
    );
    if (!existing.empty()) {
        throw MemberAlreadyInClanException(userId, clanId);
    }
    ClanMember cm{-1, clanId, userId};
    storage.insert(cm);
}

void ClanManager::removeMember(int clanId, int userId) {
    Clan clan = getClan(clanId);
    if (clan.leader_id == userId) {
        throw CannotRemoveLeaderException();
    }
    auto members = storage.get_all<ClanMember>(
        where(c(&ClanMember::clan_id) == clanId
              and c(&ClanMember::user_id) == userId)
    );
    if (members.empty()) {
        throw NotMemberException(userId, clanId);
    }
    storage.remove<ClanMember>(members.front().id);
}

void ClanManager::transferLeadership(int clanId, int newLeaderId) {
    Clan clan = getClan(clanId);
    auto members = storage.get_all<ClanMember>(
        where(c(&ClanMember::clan_id) == clanId
              and c(&ClanMember::user_id) == newLeaderId)
    );
    if (members.empty()) {
        throw LeaderNotInClanException(newLeaderId, clanId);
    }
    clan.leader_id = newLeaderId;
    storage.update(clan);
}

std::vector<Clan> ClanManager::getActiveClans() const {
    return storage.get_all<Clan>(where(c(&Clan::deleted_at) == 0));
}

std::vector<Clan> ClanManager::getAllClans() const {
    return storage.get_all<Clan>();
}

std::vector<ClanMember> ClanManager::getClanMembers(int clanId) const {
    // 不校验战队是否活跃，可能想看已解散战队的成员列表？这里选择返回所有成员
    return storage.get_all<ClanMember>(
        where(c(&ClanMember::clan_id) == clanId)
    );
}

Clan ClanManager::getClan(int clanId) const {
    auto clans = storage.get_all<Clan>(
        where(c(&Clan::id) == clanId and c(&Clan::deleted_at) == 0)
    );
    if (clans.empty()) {
        // 尝试查询是否存在但已删除
        auto all = storage.get_all<Clan>(where(c(&Clan::id) == clanId));
        if (all.empty()) {
            throw ClanNotFoundException(clanId);
        } else {
            throw ClanNotActiveException(clanId);
        }
    }
    return clans.front();
}

bool ClanManager::isClanActive(int clanId) const {
    auto clans = storage.get_all<Clan>(
        where(c(&Clan::id) == clanId and c(&Clan::deleted_at) == 0)
    );
    return !clans.empty();
}

} // namespace app
