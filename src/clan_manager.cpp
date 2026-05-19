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

int ClanManager::applyToClan(int clanId, int userId) {
    // 1. 验证用户活跃
    if (!userMgr.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    // 2. 验证战队活跃
    Clan clan = getClan(clanId); // 自动检查活跃性

    // 3. 检查是否已是成员
    auto members = getClanMembers(clanId);
    for (const auto& m : members) {
        if (m.user_id == userId) {
            throw AlreadyMemberException(userId, clanId);
        }
    }

    // 4. 检查是否已有待审批申请
    auto existing = storage.get_all<ClanApplication>(
        where(c(&ClanApplication::clan_id) == clanId
              and c(&ClanApplication::applicant_id) == userId
              and c(&ClanApplication::status) == 0)  // pending only
    );
    if (!existing.empty()) {
        throw AlreadyAppliedException(userId, clanId);
    }

    // 5. 创建申请
    ClanApplication app;
    app.clan_id = clanId;
    app.applicant_id = userId;
    app.status = 0; // pending
    app.created_at = getCurrentTimestamp();
    return storage.insert(app);
}

void ClanManager::processApplication(int applicationId, int handlerId, const std::string& action) {
    // 1. 查找申请
    auto apps = storage.get_all<ClanApplication>(
        where(c(&ClanApplication::id) == applicationId and c(&ClanApplication::status) == 0)
    );
    if (apps.empty()) {
        throw ApplicationNotFoundException(applicationId);
    }
    ClanApplication app = apps.front();

    // 2. 验证处理人是该战队领导者
    Clan clan = getClan(app.clan_id);
    if (clan.leader_id != handlerId) {
        throw NotAuthorizedException();
    }

    // 3. 事务处理（批准时需要添加成员）
    auto guard = storage.transaction_guard();

    if (action == "approve") {
        // 检查用户是否还在活跃状态
        if (!userMgr.isUserActive(app.applicant_id)) {
            throw UserNotFoundException("ID " + std::to_string(app.applicant_id));
        }
        // 检查是否已在战队（可能同时被别的途径加入）
        auto members = getClanMembers(app.clan_id);
        bool alreadyMember = false;
        for (const auto& m : members) {
            if (m.user_id == app.applicant_id) {
                alreadyMember = true;
                break;
            }
        }
        if (alreadyMember) {
            throw AlreadyMemberException(app.applicant_id, app.clan_id);
        }

        // 添加成员
        ClanMember cm{-1, app.clan_id, app.applicant_id};
        storage.insert(cm);

        app.status = 1; // approved
        app.updated_at = getCurrentTimestamp();
        storage.update(app);
    } else if (action == "reject") {
        app.status = 2; // rejected
        app.updated_at = getCurrentTimestamp();
        storage.update(app);
    } else {
        throw AppException("Invalid action: " + action);
    }

    guard.commit();
}

std::vector<ClanApplication> ClanManager::getMyApplications(int userId, std::optional<int> statusFilter) const {
    if (statusFilter.has_value()) {
        return storage.get_all<ClanApplication>(
            where(c(&ClanApplication::applicant_id) == userId
                  and c(&ClanApplication::status) == statusFilter.value()),
            order_by(&ClanApplication::created_at).desc()
        );
    } else {
        return storage.get_all<ClanApplication>(
            where(c(&ClanApplication::applicant_id) == userId),
            order_by(&ClanApplication::created_at).desc()
        );
    }
}

std::vector<ClanApplication> ClanManager::getPendingApplications(int clanId, int leaderId) const {
    // 验证领导者身份
    Clan clan = getClan(clanId);
    if (clan.leader_id != leaderId) {
        throw NotAuthorizedException();
    }
    return storage.get_all<ClanApplication>(
        where(c(&ClanApplication::clan_id) == clanId and c(&ClanApplication::status) == 0),
        order_by(&ClanApplication::created_at).asc()
    );
}

} // namespace app
