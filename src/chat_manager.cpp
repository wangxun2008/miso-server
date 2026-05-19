#include "chat_manager.h"
#include "user_manager.h"
#include "clan_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

ChatManager::ChatManager(Storage& storage, UserManager& userMgr, ClanManager& clanMgr)
    : storage(storage), userMgr(userMgr), clanMgr(clanMgr) {}

int ChatManager::sendGlobalMessage(int senderId, const std::string& content) {
    if (!userMgr.isUserActive(senderId)) {
        throw UserNotFoundException("ID " + std::to_string(senderId));
    }

    Message msg;
    msg.sender_id = senderId;
    msg.target_type = 0;            // global
    msg.target_id = std::nullopt;
    msg.content = content;
    msg.created_at = getCurrentTimestamp();
    return storage.insert(msg);
}

int ChatManager::sendClanMessage(int senderId, int clanId, const std::string& content) {
    // 验证发送者
    if (!userMgr.isUserActive(senderId)) {
        throw UserNotFoundException("ID " + std::to_string(senderId));
    }
    // 验证战队存在且活跃
    Clan clan = clanMgr.getClan(clanId); // 会抛出 ClanNotActiveException 或 ClanNotFoundException

    // 验证发送者是战队成员
    auto members = clanMgr.getClanMembers(clanId);
    bool isMember = false;
    for (const auto& m : members) {
        if (m.user_id == senderId) {
            isMember = true;
            break;
        }
    }
    if (!isMember) {
        throw NotMemberOfClanException(senderId, clanId);
    }

    Message msg;
    msg.sender_id = senderId;
    msg.target_type = 1;            // clan
    msg.target_id = clanId;
    msg.content = content;
    msg.created_at = getCurrentTimestamp();
    return storage.insert(msg);
}

std::vector<Message> ChatManager::getGlobalMessages(int limit, int offset) const {
    return storage.get_all<Message>(
        where(c(&Message::target_type) == 0),
        order_by(&Message::created_at).desc(),
        sqlite_orm::limit(limit, sqlite_orm::offset(offset))
    );
}

std::vector<Message> ChatManager::getClanMessages(int clanId, int userId, int limit, int offset) const {
    // 查看者必须是战队成员（或至少是活跃用户？这里强制要求是成员）
    auto members = clanMgr.getClanMembers(clanId);
    bool isMember = false;
    for (const auto& m : members) {
        if (m.user_id == userId) {
            isMember = true;
            break;
        }
    }
    if (!isMember) {
        throw NotMemberOfClanException(userId, clanId);
    }

    return storage.get_all<Message>(
        where(c(&Message::target_type) == 1 and c(&Message::target_id) == clanId),
        order_by(&Message::created_at).desc(),
        sqlite_orm::limit(limit, sqlite_orm::offset(offset))
    );
}

} // namespace app
