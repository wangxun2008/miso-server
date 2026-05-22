#include "online_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

OnlineManager::OnlineManager(Storage& storage, UserManager& userMgr)
    : storage(storage), userMgr(userMgr) {}

void OnlineManager::userOnline(int userId) {
    // 仅活跃用户可以上线
    if (!userMgr.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }

    auto sessions = storage.get_all<UserSession>(
        where(c(&UserSession::user_id) == userId)
    );

    int64_t now = getCurrentTimestamp();
    if (sessions.empty()) {
        UserSession s;
        s.user_id = userId;
        s.is_online = true;
        s.last_heartbeat = now;
        storage.insert(s);
    } else {
        UserSession s = sessions.front();
        s.is_online = true;
        s.last_heartbeat = now;
        storage.update(s);
    }
}

void OnlineManager::userOffline(int userId) {
    // 允许离线操作，即使已离线也没关系（幂等）
    auto sessions = storage.get_all<UserSession>(
        where(c(&UserSession::user_id) == userId)
    );
    if (!sessions.empty()) {
        UserSession s = sessions.front();
        s.is_online = false;
        storage.update(s);
    }
}

void OnlineManager::updateHeartbeat(int userId) {
    auto sessions = storage.get_all<UserSession>(
        where(c(&UserSession::user_id) == userId and c(&UserSession::is_online) == true)
    );
    if (sessions.empty()) {
        throw UserOfflineException(userId);  // 不能对离线用户更新心跳
    }
    UserSession s = sessions.front();
    s.last_heartbeat = getCurrentTimestamp();
    storage.update(s);
}

bool OnlineManager::isUserOnline(int userId, int timeoutSeconds) const {
    int64_t threshold = getCurrentTimestamp() - timeoutSeconds;
    auto sessions = storage.get_all<UserSession>(
        where(c(&UserSession::user_id) == userId
              and c(&UserSession::is_online) == true
              and c(&UserSession::last_heartbeat) >= threshold)
    );
    return !sessions.empty();
}

std::vector<int> OnlineManager::getOnlineUsers(int timeoutSeconds) const {
    int64_t threshold = getCurrentTimestamp() - timeoutSeconds;
    auto sessions = storage.get_all<UserSession>(
        where(c(&UserSession::is_online) == true
              and c(&UserSession::last_heartbeat) >= threshold)
    );
    std::vector<int> userIds;
    for (const auto& s : sessions) {
        userIds.push_back(s.user_id);
    }
    return userIds;
}

} // namespace app
